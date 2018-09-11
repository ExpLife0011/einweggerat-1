#ifndef CEMULATOR_H
#define CEMULATOR_H
#include <initguid.h>
#include <list>
#include <thread>
#include <chrono>
#include <mutex>
#include "io/input.h"
#include "io/audio.h"

namespace std
{
  typedef wstring        tstring;
  typedef wistringstream tistringstream;
  typedef wostringstream tostringstream;
}

class CLibretro
{
  static	CLibretro* m_Instance;
public:
   CLibretro();
   ~CLibretro();
   static CLibretro* CreateInstance(HWND hwnd);
   static	CLibretro* GetSingleton();

  struct core_vars
  {
    char name[1024];
    char var[1024];
    char description[1024];
    char usevars[1024];
  };
  bool gamespec;
  TCHAR core_path[MAX_PATH];
  TCHAR rom_path[MAX_PATH];
  TCHAR inputcfg_path[MAX_PATH];
  TCHAR sys_filename[MAX_PATH];
  TCHAR sav_filename[MAX_PATH];
  TCHAR corevar_path[MAX_PATH];
  Audio  _audio;
  std::vector<core_vars> variables;
  struct retro_game_info info;
  bool variables_changed;
  HANDLE thread_handle;
  DWORD thread_id;
  HWND emulator_hwnd;
  DWORD rate;
  double lastTime;
  int nbFrames;
  bool threaded;
  BOOL isEmulating;
  retro_usec_t runloop_frame_time_last;

  DWORD ThreadStart();
  bool running();
  bool loadfile(TCHAR* filename, TCHAR* core_filename, bool gamespecificoptions, bool mthreaded = false);
  void render();
  void run();
  void reset();
  bool init_common();
  bool core_load(TCHAR *sofile, bool specifics, TCHAR* filename);
  bool init(HWND hwnd);
  bool savestate(TCHAR* filename, bool save = false);
  bool savesram(TCHAR* filename, bool save = false);
  void kill();
  void core_audio_sample(int16_t left, int16_t right);
  size_t core_audio_sample_batch(const int16_t *data, size_t frames);
};


#endif CEMULATOR_H