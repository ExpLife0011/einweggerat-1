#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#include <thread>
#include <chrono>
#include <mutex>
#include "io/input.h"
#include "io/audio.h"
#define MUDLIB_IMPLEMENTATION
#include "mudlib.h"

namespace std
{
  typedef wstring        tstring;
  typedef wistringstream tistringstream;
  typedef wostringstream tostringstream;
}

#ifdef __cplusplus
extern "C" {
#endif
    void load_envsymb(HMODULE handle);
    void save_coresettings();
#ifdef __cplusplus
}
#endif

class CLibretro
{
    static	CLibretro* m_Instance;
public:
    CLibretro();
    ~CLibretro();
    static CLibretro* GetInstance(HWND hwnd = NULL);

    struct core_vars
    {
        std::string name;
        std::string var;
        std::string description;
        std::string usevars;
    };

    struct {
        HMODULE handle;
        bool initialized;
        void(*retro_init)(void);
        void(*retro_deinit)(void);
        unsigned(*retro_api_version)(void);
        void(*retro_get_system_info)(struct retro_system_info *info);
        void(*retro_get_system_av_info)(struct retro_system_av_info *info);
        void(*retro_set_controller_port_device)(unsigned port, unsigned device);
        void(*retro_reset)(void);
        void(*retro_run)(void);
        size_t(*retro_serialize_size)(void);
        bool(*retro_serialize)(void *data, size_t size);
        bool(*retro_unserialize)(const void *data, size_t size);
        bool(*retro_load_game)(const struct retro_game_info *game);
        void *(*retro_get_memory_data)(unsigned id);
        size_t(*retro_get_memory_size)(unsigned id);
        void(*retro_unload_game)(void);
    } g_retro;

    bool gamespec;
    TCHAR exe_dir[MAX_PATH];
    TCHAR save_name[MAX_PATH];
    TCHAR core_config[MAX_PATH];
    TCHAR input_config[MAX_PATH];
    Audio  _audio;
    std::vector<core_vars> variables;
    struct retro_game_info info;
    bool variables_changed;
    HWND emulator_hwnd;
    DWORD rate;
    double lastTime;
    int nbFrames;
    BOOL isEmulating;
    retro_usec_t  frame_limit_last_time;
    retro_usec_t  runloop_frame_time_last;
    retro_time_t frame_limit_minimum_time;
    Mud_Timing mudtime;

    bool running();
    bool loadfile(TCHAR* filename, TCHAR* core_filename, bool gamespecificoptions);
    void render();
    void run();
    void reset();
    bool core_load(TCHAR *sofile, bool specifics, TCHAR* filename);
    void core_unload();
    bool init(HWND hwnd);
    bool savestate(TCHAR* filename, bool save = false);
    bool savesram(TCHAR* filename, bool save = false);
    void kill();

};

#endif CEMULATOR_H