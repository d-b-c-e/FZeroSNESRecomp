/* Production consumer with fake Win32 loader + fake exports. Never loads a
 * DLL, initializes SDL/DirectInput, registers exit handlers or emits torque. */
#define NOMINMAX
#include <windows.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>

static HMODULE WINAPI fake_load(LPCWSTR);
static HMODULE WINAPI fake_self(LPCWSTR);
static DWORD WINAPI fake_path(HMODULE, LPWSTR, DWORD);
static FARPROC WINAPI fake_export(HMODULE, LPCSTR);
static BOOL WINAPI fake_unload(HMODULE);
#define LoadLibraryW fake_load
#define GetModuleHandleW fake_self
#define GetModuleFileNameW fake_path
#define GetProcAddress fake_export
#define FreeLibrary fake_unload
#include "../src/fzero_ffb.cpp"
#undef LoadLibraryW
#undef GetModuleHandleW
#undef GetModuleFileNameW
#undef GetProcAddress
#undef FreeLibrary

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::abort(); } } while (0)
static std::mutex events_mutex;
static std::condition_variable events_cv;
static std::vector<std::string> events;
static int loads, unloads;
static bool missing_export, block_frame, frame_entered, release_frame;
static bool shutdown_requested, free_entered, release_shutdown, worker_active;
static bool shutdown_completed;
static void note(const char *name) {
  std::unique_lock<std::mutex> lock(events_mutex);
  events.emplace_back(name);
  if (!std::strcmp(name,"SetHoldTimeoutMs")) worker_active = true;
  if (!std::strcmp(name,"SetDeviceForcesXY") && block_frame) {
    frame_entered = true; events_cv.notify_all();
    CHECK(events_cv.wait_for(lock,std::chrono::seconds(5),[]{return release_frame;}));
  }
  if (!std::strcmp(name,"FreeDirectInput")) {
    free_entered = true; events_cv.notify_all();
    CHECK(events_cv.wait_for(lock,std::chrono::seconds(5),[]{return release_shutdown;}));
    worker_active = false; shutdown_completed = true;
    events.emplace_back("ShutdownReturned");
  }
}
template<class T> static T result(const char *name) { note(name); return (T)1; }
template<> void result<void>(const char *name) { note(name); }
#define FAKE_EXPORT(ret,name,args) static ret __cdecl stub_##name args { return result<ret>(#name); }
WHEELFFB_API_LIST(FAKE_EXPORT)
#undef FAKE_EXPORT
static int __cdecl device_name(int, char *name, int cap) {
  note("GetDeviceName"); std::snprintf(name,(size_t)cap,"FAKE WHEEL"); return 1;
}
static int __cdecl device_guid(int, void *guid) {
  note("GetDeviceGuid"); std::memset(guid,1,16); return 1;
}
static HMODULE WINAPI fake_load(LPCWSTR) {
  std::lock_guard<std::mutex> lock(events_mutex); ++loads; return (HMODULE)1;
}
static HMODULE WINAPI fake_self(LPCWSTR) { return (HMODULE)2; }
static DWORD WINAPI fake_path(HMODULE, LPWSTR path, DWORD cap) {
  const wchar_t *value=L"C:\\fake\\consumer.exe";
  CHECK(cap>wcslen(value)); wcscpy_s(path,cap,value); return (DWORD)wcslen(value);
}
static FARPROC WINAPI fake_export(HMODULE, LPCSTR name) {
  if (missing_export && !std::strcmp(name,"GetLastHResult")) return nullptr;
  if (!std::strcmp(name,"GetDeviceName")) return (FARPROC)device_name;
  if (!std::strcmp(name,"GetDeviceGuid")) return (FARPROC)device_guid;
#define RESOLVE(ret,export_name,args) if (!std::strcmp(name,#export_name)) return (FARPROC)stub_##export_name;
  WHEELFFB_API_LIST(RESOLVE)
#undef RESOLVE
  return nullptr; /* Optional constant-burst extensions deliberately absent. */
}
static BOOL WINAPI fake_unload(HMODULE) {
  std::lock_guard<std::mutex> lock(events_mutex);
  CHECK(!worker_active); ++unloads; events.emplace_back("FreeLibrary"); return TRUE;
}
static size_t event_count() {
  std::lock_guard<std::mutex> lock(events_mutex); return events.size();
}
int main() {
  char names[2][256]{};
  missing_export=true;
  CHECK(FzeroFfbListDevices(names,2)==0 && loads==1 && unloads==1);
  missing_export=false;
  FILE *cfg=std::fopen("fake-ffb-lifecycle.ini","wb"); CHECK(cfg);
  std::fputs("[ForceFeedback]\nEnabled=1\nDevice=FAKE WHEEL\nImpactType=Sine\n",cfg);
  std::fclose(cfg);
  FzeroFfbInit("fake-ffb-lifecycle.ini",nullptr);
  CHECK(worker_active);
  unsigned char ram[0x20000]{};
  ram[0x54]=2; ram[0x55]=3;
  block_frame=true;
  std::thread frame([&]{FzeroFfbFrame(ram,sizeof(ram),0);});
  {
    std::unique_lock<std::mutex> lock(events_mutex);
    CHECK(events_cv.wait_for(lock,std::chrono::seconds(5),[]{return frame_entered;}));
  }
  std::thread shutdown([]{
    { std::lock_guard<std::mutex> lock(events_mutex); shutdown_requested=true; events_cv.notify_all(); }
    FzeroFfbShutdown();
  });
  {
    std::unique_lock<std::mutex> lock(events_mutex);
    CHECK(events_cv.wait_for(lock,std::chrono::seconds(5),[]{return shutdown_requested;}));
    CHECK(!free_entered && worker_active); /* In-flight frame owns call gate. */
    release_frame=true; events_cv.notify_all();
    CHECK(events_cv.wait_for(lock,std::chrono::seconds(5),[]{return free_entered;}));
    CHECK(unloads==1 && worker_active && !shutdown_completed);
  }
  frame.join();
  size_t closed_count=event_count();
  std::thread late_frame([&]{FzeroFfbFrame(ram,sizeof(ram),0);});
  std::thread late_silence([]{FzeroFfbSilence();});
  std::thread late_discovery([&]{CHECK(FzeroFfbListDevices(names,2)==0);});
  {
    std::lock_guard<std::mutex> lock(events_mutex);
    CHECK(events.size()==closed_count && unloads==1);
    release_shutdown=true; events_cv.notify_all();
  }
  shutdown.join(); late_frame.join(); late_silence.join(); late_discovery.join();
  CHECK(shutdown_completed && !worker_active && unloads==2 && loads==2);
  CHECK(event_count()==closed_count+2); /* ShutdownReturned then FreeLibrary only. */
  CHECK(events[events.size()-2]=="ShutdownReturned" && events.back()=="FreeLibrary");
  size_t final_count=event_count();
  FzeroFfbShutdown(); FzeroFfbSilence(); FzeroFfbFrame(ram,sizeof(ram),0);
  CHECK(FzeroFfbListDevices(names,2)==0 && event_count()==final_count && loads==2);
  std::remove("fake-ffb-lifecycle.ini");
  std::puts("Consumer drain, close, completed-shutdown/unload and late-call gates passed (fake backend only)");
}
