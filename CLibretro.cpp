#include "stdafx.h"
#include <windows.h>
#include "CLibretro.h"
#include "3rdparty/libretro.h"

#define MUDLIB_IMPLEMENTATION
#include "mudlib.h"

#include "io/gl_render.h"
#include "gui/utf8conv.h"
#define INI_IMPLEMENTATION
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


bool CLibretro::savestate(TCHAR* filename, bool save) {
    if (isEmulating)
    {
        size_t size = g_retro.retro_serialize_size();
        if (size)
        {
            BYTE *Memory = (BYTE *)malloc(size);
            if (save)
            {
                // Get the filesize
                g_retro.retro_serialize(Memory, size);
                if (Memory)Mud_FileAccess::save_data(Memory, size, filename);
                free(Memory);
            }
            else
            {
                unsigned sz;
                BYTE* save_data = (BYTE*)Mud_FileAccess::load_data(filename, &sz);
                if (!save_data)
                {
                    free(Memory);
                    return false;
                }
                memcpy(Memory, save_data, size);
                g_retro.retro_unserialize(Memory, size);
                free(save_data);
                free(Memory);
            }
            return true;
        }
    }
    return false;
}

bool CLibretro::savesram(TCHAR* filename, bool save) {
    if (isEmulating)
    {
        size_t size = g_retro.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
        if (size)
        {
            BYTE *Memory = (BYTE *)g_retro.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
            if (save)
            {
                return Mud_FileAccess::save_data(Memory, size, filename);
            }
            else
            {
                unsigned sz;
                BYTE* save_data = (BYTE*)Mud_FileAccess::load_data(filename, &sz);
                if (!save_data)return false;
                memcpy(Memory, save_data, size);
                free(save_data);
                return true;
            }
        }
    }
    return false;
}

void CLibretro::reset() {
    if (isEmulating)g_retro.retro_reset();
}

void CLibretro::core_unload() {
    isEmulating = false;
    if (g_retro.initialized)
    {
        g_retro.retro_unload_game();
        g_retro.retro_deinit();
    }
    if (g_retro.handle)
    {
        FreeLibrary(g_retro.handle);
        g_retro.handle = NULL;
    }
    memset(&g_retro, 0, sizeof(g_retro));
}

bool CLibretro::core_load(TCHAR *sofile, bool gamespecificoptions, TCHAR* game_filename) {
    TCHAR game_filename_[MAX_PATH] = { 0 };
    ZeroMemory(core_config, sizeof(TCHAR)*MAX_PATH);
    ZeroMemory(savesys_name, sizeof(TCHAR)*MAX_PATH);
    ZeroMemory(input_config, sizeof(TCHAR)*MAX_PATH);

    g_retro.handle = LoadLibrary(sofile);
    if (!g_retro.handle)return false;
#define die() do { FreeLibrary(g_retro.handle); return false; } while(0)
#define libload(name) GetProcAddress(g_retro.handle, name)
#define load_sym(V,name) if (!(*(void**)(&V)=(void*)libload(#name))) die()
#define load_retro_sym(S) load_sym(g_retro.S, S)
    load_retro_sym(retro_init);
    load_retro_sym(retro_deinit);
    load_retro_sym(retro_api_version);
    load_retro_sym(retro_get_system_info);
    load_retro_sym(retro_get_system_av_info);
    load_retro_sym(retro_set_controller_port_device);
    load_retro_sym(retro_reset);
    load_retro_sym(retro_run);
    load_retro_sym(retro_load_game);
    load_retro_sym(retro_unload_game);
    load_retro_sym(retro_serialize);
    load_retro_sym(retro_unserialize);
    load_retro_sym(retro_serialize_size);
    load_retro_sym(retro_get_memory_size);
    load_retro_sym(retro_get_memory_data);


    lstrcpy(game_filename_, game_filename);
    PathStripPath(game_filename_);
    PathRemoveExtension(game_filename_);


    TCHAR einweg_dir[MAX_PATH] = { 0 };
    GetCurrentDirectory(MAX_PATH, einweg_dir);
    PathAppend(einweg_dir, L"system");
    lstrcpy(savesys_name, einweg_dir);
    PathAppend(savesys_name, game_filename_);

  
   

    if (gamespecificoptions)
    {
        TCHAR core_path[MAX_PATH] = { 0 };
        lstrcpy(core_path, sofile);
        PathRemoveExtension(core_path);
        lstrcpy(core_config, core_path);
        PathAppend(core_config, game_filename_);
        lstrcat(core_config, L".ini");
    }
    else
    {
        lstrcpy(core_config, sofile);
        PathRemoveExtension(core_config);
        lstrcpy(input_config, core_config);
        lstrcat(core_config, L".ini");
        lstrcat(input_config, L".cfg");
    }
    //set libretro func pointers
    load_envsymb(g_retro.handle);
    g_retro.retro_init();
    g_retro.initialized = true;
    return true;
}

static void noop() {
}


//////////////////////////////////////////////////////////////////////////////////////////
CLibretro* CLibretro::m_Instance = 0;
CLibretro* CLibretro::GetInstance(HWND hwnd) {
    if (0 == m_Instance)
    {
        m_Instance = new CLibretro();
        m_Instance->init(hwnd);
    }
    return m_Instance;
}

bool CLibretro::running() {
    return isEmulating;
}

CLibretro::CLibretro() {
    memset(&g_retro, 0, sizeof(g_retro));
    isEmulating = false;
}

CLibretro::~CLibretro(void) {
    if (isEmulating)isEmulating = false;
    kill();
}

bool CLibretro::loadfile(TCHAR* filename, TCHAR* core_filename, bool gamespecificoptions)
{
  
    if (isEmulating)
    {
        kill();
        isEmulating = false;
    }
   
    double refreshr = 0;
    DEVMODE lpDevMode;
    struct retro_system_info system = { 0 };
    retro_system_av_info av = { 0 };
    variables.clear();
    memset(&lpDevMode, 0, sizeof(DEVMODE));
    variables_changed = false;

    g_video = { 0 };
    g_video.hw.version_major = 4;
    g_video.hw.version_minor = 5;
    g_video.hw.context_type = RETRO_HW_CONTEXT_NONE;
    g_video.hw.context_reset = NULL;
    g_video.hw.context_destroy = NULL;

    if (!core_load(core_filename, gamespecificoptions, filename))
    {
        printf("FAILED TO LOAD CORE!!!!!!!!!!!!!!!!!!");
        return false;
    }

    string ansi = utf8_from_utf16(filename);
    const char* rompath = ansi.c_str();
    info = { rompath, 0 };
    info.path = rompath;
    info.data = NULL;
    info.size = Mud_FileAccess::get_filesize(filename);
    info.meta = "";
    g_retro.retro_get_system_info(&system);
    if (!system.need_fullpath) {
        FILE *inputfile = _wfopen(filename, L"rb");
        if (!inputfile)
        {
        fail:
            printf("FAILED TO LOAD ROMz!!!!!!!!!!!!!!!!!!");
            return false;
        }
        info.data = malloc(info.size);
        if (!info.data)goto fail;
        size_t read = fread((void*)info.data, 1, info.size, inputfile);
        fclose(inputfile);
        inputfile = NULL;
    }
    if (!g_retro.retro_load_game(&info))
    {
        printf("FAILED TO LOAD ROM!!!!!!!!!!!!!!!!!!");
        return false;
    }
    g_retro.retro_get_system_av_info(&av);
    // g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    ::video_init(&av.geometry, emulator_hwnd);
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode) == 0) {
        refreshr = 60.0; // default value if cannot retrieve from user settings.
    }
    else refreshr = lpDevMode.dmDisplayFrequency;
    _audio.init(refreshr, av);
    lastTime = (double)mudtime.milliseconds_now() / 1000;
    nbFrames = 0;
    isEmulating = true;
    frame_limit_last_time = 0;
    runloop_frame_time_last = 0;
    frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f / av.timing.fps * 1.0f);
    return true;
}

void CLibretro::run()
{
    if (!g_video.software_rast)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
   

    g_retro.retro_run();

    double currentTime = (double)mudtime.milliseconds_now() / 1000;
    if (currentTime - lastTime >= 0.5) { // If last prinf() was more than 1 sec ago
                       // printf and reset timer
        TCHAR buffer[200] = { 0 };
        int len = swprintf(buffer, 200, L"einweggerät: %2f ms/frame\n, min %d VPS", 1000.0 / double(nbFrames), nbFrames);
        SetWindowText(emulator_hwnd, buffer);
        nbFrames = 0;
        lastTime += 1.0;
    }
    nbFrames++;
}

bool CLibretro::init(HWND hwnd)
{
    isEmulating = false;
    emulator_hwnd = hwnd;
    return true;
}

void CLibretro::kill()
{

    if (isEmulating)
    {
        isEmulating = false;
        core_unload();
        if (info.data)
            free((void*)info.data);
        _audio.destroy();
        video_deinit();
    }
}