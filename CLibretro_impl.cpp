#include "stdafx.h"
#include <windows.h>
#include "CLibretro.h"
#include "3rdparty/libretro.h"

#include "mudlib.h"
#include "io/gl_render.h"
#include "gui/utf8conv.h"
#define INI_STRNICMP( s1, s2, cnt ) (strcmp( s1, s2) )
#include "3rdparty/ini.h"
#include <algorithm>
#include <numeric>  
#include <sys/stat.h>
#include <Shlwapi.h>
#include <dwmapi.h>

#define INLINE 
using namespace std;
using namespace utf8util;


static struct retro_frame_time_callback runloop_frame_time;
static struct retro_audio_callback audio_callback;


unsigned char* load_inputsettings(TCHAR* path, unsigned * size)
{
    FILE *fp = _wfopen(path, L"r");
    if (fp)
    {
        fseek(fp, 0, SEEK_END);
        int size_ = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char* data = (char*)malloc(size_ + 1);
        fread(data, 1, size_, fp);
        data[size_] = '\0';
        fclose(fp);
        ini_t* ini = ini_load(data, NULL);
        free(data);
        int section = ini_find_section(ini, "Input Settings", strlen("Input Settings"));
        if (section == INI_NOT_FOUND)
        {
            ini_destroy(ini);
            free(data);
            fclose(fp);
            return NULL;
        }
        int index = ini_find_property(ini, section, "Data", strlen("Data"));
        char const* text = ini_property_value(ini, section, index);
        tstring ansi = utf16_from_utf8(text);
        unsigned len = 0;
        unsigned char * data2 = Mud_Base64::decode(ansi.c_str(), &len);
        *size = len;
        free(data);
        ini_destroy(ini);
        return data2;
    }
    return NULL;
}

const char* load_coresettings(retro_variable *var) {
    CLibretro *retro = CLibretro::GetInstance();
    for (int i = 0; i < retro->variables.size(); i++)
    {
        if (strcmp(retro->variables[i].name.c_str(), var->key) == 0)
        {
            return retro->variables[i].var.c_str();
        }
    }
    return NULL;
}


void save_coresettings()
{
    CLibretro *retro = CLibretro::GetInstance();
    FILE *fp  = _wfopen(retro->core_config, L"r+");
    ini_t* ini = NULL;
    char* data = NULL;
    if (fp)
    {
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        data = (char*)malloc(size + 1);
        fread(data, 1, size, fp);
        rewind(fp);
        data[size] = '\0';
        ini = ini_load(data, NULL);
        for (int i = 0; i < retro->variables.size(); i++) {

            int idx = ini_find_property(ini, INI_GLOBAL_SECTION, (char*)retro->variables[i].name.c_str(),
                strlen(retro->variables[i].name.c_str()));
                ini_property_value_set(ini, INI_GLOBAL_SECTION, idx,
                    retro->variables[i].var.c_str(), strlen(retro->variables[i].var.c_str()));
        }
       
        free(data);
        size = ini_save(ini, NULL, 0); // Find the size needed
        data = (char*)malloc(size);
        size = ini_save(ini, data, size); // Actually save the file	
        fwrite(data, 1, size, fp);
    }
    else
    {
        ini = ini_create(NULL);
        for (int i = 0; i < retro->variables.size(); i++) {
                ini_property_add(ini, INI_GLOBAL_SECTION, retro->variables[i].name.c_str(), strlen(retro->variables[i].name.c_str()), retro->variables[i].var.c_str(),
                    strlen(retro->variables[i].var.c_str()));
        }
        int size = ini_save(ini, NULL, 0); // Find the size needed
        data = (char*)malloc(size);
        size = ini_save(ini, data, size); // Actually save the file	
        fwrite(data, 1, size, fp);
     
    }
    ini_destroy(ini);
    fclose(fp);
    free(data);
}

void init_coresettings(retro_variable *var) {
    CLibretro * retro = CLibretro::GetInstance();
    FILE *fp = NULL;
    std::vector<CLibretro::core_vars> variables;
    //set up core variable information and default key settings
    while (var->value != NULL)
    {
        CLibretro::core_vars vars_struct;
        vars_struct.name = var->key;
        char descript_var[50] = { 0 };
        char *e = strchr((char*)var->value, ';');
        strncpy(descript_var, var->value, (int)(e - (char*)var->value));
        vars_struct.description = descript_var;
        char * pch = strstr((char*)var->value, (char*)"; ");
        pch += strlen("; ");
        vars_struct.usevars = pch;
        char* str2 = strstr(pch, (char*)"|");
        int len = !str2 ? strlen(pch) : str2 - pch;
        strncpy(descript_var, pch, len);
        vars_struct.var.append(descript_var, len);
        variables.push_back(vars_struct);
        var++;
    }

    fp = _wfopen(retro->core_config, L"r");
    if (!fp)
    {
        //create a new file with defaults
        ini_t * ini = ini_create(NULL);
        for (int i = 0; i < variables.size(); i++)
        {
            ini_property_add(ini, INI_GLOBAL_SECTION, (char*)variables[i].name.c_str(),
                strlen(variables[i].name.c_str()), (char*)variables[i].var.c_str(), strlen(variables[i].var.c_str()));
            retro->variables.push_back(variables[i]);
        }
        int size = ini_save(ini, NULL, 0); // Find the size needed
        char* data = (char*)malloc(size);
        size = ini_save(ini, data, size); // Actually save the file
        ini_destroy(ini);
        fp = _wfopen(retro->core_config, L"w");
        fwrite(data, 1, size, fp);
        fclose(fp);
        free(data);
    }
    else
    {
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char* data = (char*)malloc(size + 1);
        fread(data, 1, size, fp);
        data[size] = '\0';
        fclose(fp);
        ini_t* ini = ini_load(data, NULL);
        free(data);
        int num_vars = variables.size();
        int vars_infile = ini_property_count(ini, INI_GLOBAL_SECTION);
        if (vars_infile != num_vars) {
            fclose(fp);
        }
        bool save = false;
        for (int i = 0; i < num_vars; i++)
        {
            int idx = ini_find_property(ini, INI_GLOBAL_SECTION, (char*)variables[i].name.c_str(),
                strlen(variables[i].name.c_str()));
            if (idx != INI_NOT_FOUND)
            {
                const char* variable_val = ini_property_value(ini, INI_GLOBAL_SECTION, idx);
                variables[i].var = variable_val;
                retro->variables.push_back(variables[i]);
            }
            else
            {
                ini_property_add(ini, INI_GLOBAL_SECTION, (char*)variables[i].name.c_str(), strlen(variables[i].name.c_str()),
                    (char*)variables[i].var.c_str(), strlen(variables[i].var.c_str()));
                retro->variables.push_back(variables[i]);
                save = true;
            }
        }
        if (save)
        {
            int size = ini_save(ini, NULL, 0); // Find the size needed
            char* data = (char*)malloc(size);
            size = ini_save(ini, data, size); // Actually save the file
            fp = _wfopen(retro->core_config, L"w");
            fwrite(data, 1, size, fp);
            fclose(fp);
            free(data);
        }
        ini_destroy(ini);
        fclose(fp);
    }
}

static void core_audio_sample(int16_t left, int16_t right)
{
    CLibretro* lib = CLibretro::GetInstance();
    if (lib->isEmulating)
    {
        int16_t buf[2] = { left, right };
        lib->_audio.mix(buf, 1);
    }
}
static size_t core_audio_sample_batch(const int16_t *data, size_t frames)
{
    CLibretro* lib = CLibretro::GetInstance();
    if (lib->isEmulating)
    {
        lib->_audio.mix(data, frames);
        return frames;
    }
    return 0;
}




static void core_log(enum retro_log_level level, const char *fmt, ...) {
    char buffer[4096] = { 0 };
    char buffer2[4096] = { 0 };
    static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
    va_list va;
    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);
    if (level == 0)
        return;
    sprintf(buffer2, "[%s] %s", levelstr[level], buffer);
    fprintf(stdout, "%s", buffer2);
}

uintptr_t core_get_current_framebuffer() {
    return g_video.fbo_id;
}

bool core_environment(unsigned cmd, void *data) {
    bool *bval;
    CLibretro * retro = CLibretro::GetInstance();
    input *input_device = input::GetInstance();
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_MESSAGE: {
        struct retro_message *cb = (struct retro_message *)data;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        struct retro_log_callback *cb = (struct retro_log_callback *)data;
        cb->log = core_log;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        bval = (bool*)data;
        *bval = true;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: // 9
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: // 31
    {
        static char *sys_path = NULL;
        if (!sys_path)
        {
            string ansi = utf8_from_utf16(retro->exe_dir);
            sys_path = strdup(ansi.c_str());
        }
        char **ppDir = (char**)data;
        *ppDir = sys_path;
        return true;
    }
    break;

    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: // 31
    {
        char variable_val2[50] = { 0 };
        Std_File_Reader_u out;
        lstrcpy(input_device->path, retro->input_config);
        if (!out.open(retro->input_config))
        {
            const char *err = input_device->load(out);
            out.close();
            if (!err)
            {
                struct retro_input_descriptor *var = (struct retro_input_descriptor *)data;
                static int i = 0;
                while (var != NULL && var->port == 0) {
                    var++; i++;
                }
                if (i != input_device->bl->get_count())
                {

                    input_device->bl->clear();
                    goto init;
                }
            }
        }
        else
        {
        init:
            struct retro_input_descriptor *var = (struct retro_input_descriptor *)data;
            static int i = 0;
            while (var != NULL && var->port == 0)
            {
                dinput::di_event keyboard;
                keyboard.type = dinput::di_event::ev_none;
                keyboard.key.type = dinput::di_event::key_none;
                keyboard.key.which = NULL;
                CString str = var->description;
                int id = var->id;
                int index = var->index;
                if (var->device == RETRO_DEVICE_ANALOG || (var->device == RETRO_DEVICE_JOYPAD))
                {
                    if (var->device == RETRO_DEVICE_ANALOG)
                        id = (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) ? (var->id == RETRO_DEVICE_ID_ANALOG_X ? 16 : 17) :
                        (var->id == RETRO_DEVICE_ID_ANALOG_X ? 18 : 19);

                    input_device->bl->add(keyboard, i, str.GetBuffer(NULL), id);
                }
                i++; ++var;
            }
            Std_File_Writer_u out2;
            if (!out2.open(retro->input_config))
            {
                input_device->save(out2);
                out2.close();
            }
        }
        return true;
    }
    break;

    case RETRO_ENVIRONMENT_SET_VARIABLES:
    {
        struct retro_variable *var = (struct retro_variable *)data;
        init_coresettings(var);
        return true;
    }
    break;

    case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: {
        struct retro_audio_callback *audio_cb = (struct retro_audio_callback*)data;
        audio_callback = *audio_cb;
        return true;
    }

    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
        const struct retro_frame_time_callback *frame_time =
            (const struct retro_frame_time_callback*)data;
        runloop_frame_time = *frame_time;
        break;
    }

    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        struct retro_variable * variable = (struct retro_variable*)data;
        variable->value = load_coresettings(variable);
        return true;
    }
    break;

    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
    {
        *(bool*)data = retro->variables_changed;
        retro->variables_changed = false;
        return true;
    }
    break;

    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;
        if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
            return false;
        return video_set_pixel_format(*fmt);
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
        if (hw->context_type == RETRO_HW_CONTEXT_VULKAN)return false;
        hw->get_current_framebuffer = core_get_current_framebuffer;
        hw->get_proc_address = (retro_hw_get_proc_address_t)get_proc;
        g_video.hw = *hw;
        return true;
    }
    default:
        core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
        return false;
    }
    return false;
}

static void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    video_refresh(data, width, height, pitch);
}

static void core_input_poll(void) {
    input *input_device = input::GetInstance();
    input_device->poll();
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0)return 0;
    input *input_device = input::GetInstance();

    if (device == RETRO_DEVICE_MOUSE)
    {
        int16_t x = 0, y = 0;
        bool left = false, right = false;
        input_device->readmouse(x, y, left, right);
        switch (id)
        {
        case RETRO_DEVICE_ID_MOUSE_X:
            return x;
        case RETRO_DEVICE_ID_MOUSE_Y:
            return y;
        case RETRO_DEVICE_ID_MOUSE_LEFT:
            return left;
        case RETRO_DEVICE_ID_MOUSE_RIGHT:
            return right;
        }
        return 0;
    }

    if (device == RETRO_DEVICE_ANALOG || device == RETRO_DEVICE_JOYPAD) {
        if (input_device != NULL && input_device->bl != NULL) {
            for (unsigned int i = 0; i < input_device->bl->get_count(); i++) {
                int retro_id = 0;
                int16_t value = 0;
                bool isanalog = false;
                input_device->getbutton(i, value, retro_id, isanalog);
                if (device == RETRO_DEVICE_ANALOG)
                {
                    if (value <= -0x8000)value = -0x7fff;
                    if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
                    {
                        if ((id == RETRO_DEVICE_ID_ANALOG_X && retro_id == 16) || (id == RETRO_DEVICE_ID_ANALOG_Y && retro_id == 17))
                        {
                            return isanalog ? -value : value;
                        }
                    }
                    else
                    {
                        if ((id == RETRO_DEVICE_ID_ANALOG_X && retro_id == 18) || (id == RETRO_DEVICE_ID_ANALOG_Y && retro_id == 19))
                        {
                            return isanalog ? -value : value;
                        }
                    }
                }
                else
                {
                    value = abs(value);
                    if (retro_id == id)return value;
                }
            }
        }
    }


    return 0;
}


void load_envsymb(HMODULE handle)
{
#define libload(name) GetProcAddress(handle, name)
#define load_sym(V,name) if (!(*(void**)(&V)=(void*)libload(#name)))
    void(*set_environment)(retro_environment_t) = NULL;
    void(*set_video_refresh)(retro_video_refresh_t) = NULL;
    void(*set_input_poll)(retro_input_poll_t) = NULL;
    void(*set_input_state)(retro_input_state_t) = NULL;
    void(*set_audio_sample)(retro_audio_sample_t) = NULL;
    void(*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
    load_sym(set_environment, retro_set_environment);
    load_sym(set_video_refresh, retro_set_video_refresh);
    load_sym(set_input_poll, retro_set_input_poll);
    load_sym(set_input_state, retro_set_input_state);
    load_sym(set_audio_sample, retro_set_audio_sample);
    load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);
    set_environment(core_environment);
    set_video_refresh(core_video_refresh);
    set_input_poll(core_input_poll);
    set_input_state(core_input_state);
    set_audio_sample(core_audio_sample);
    set_audio_sample_batch(core_audio_sample_batch);
}
