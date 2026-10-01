#include "fzero_ffb.h"

extern "C" {
#include "fzero_hotkeys.h"
}

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#include "../lib/toolkit/include/wheelffb.h"
#endif

namespace {
FzeroFfbState s_state{};
int s_strength = 40;
int s_impact_strength = 20;
constexpr int kConstantImpactMs = 120;
constexpr int kSineImpactMs = 140;
constexpr int kCollisionMinEnergyLoss = 16;

#ifdef _WIN32
using CreateConstantBurstFn = int (__cdecl *)(int);
using PlayConstantBurstFn = int (__cdecl *)(int, int);
using ReleaseConstantBurstsFn = void (__cdecl *)();
WheelFfbApi s_ffb{};
int s_damper = -1;
int s_spring = -1;
int s_road = -1;
int s_collision_sine = -1;
int s_collision_constant = -1;
bool s_use_constant_impact = false;
CreateConstantBurstFn s_create_constant_burst = nullptr;
PlayConstantBurstFn s_play_constant_burst = nullptr;
ReleaseConstantBurstsFn s_release_constant_bursts = nullptr;
bool s_active = false;
unsigned s_trace_frames = 0;
bool s_trace_enabled = false;
#endif

uint16_t read16(const uint8_t *p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

int wrapped_delta(uint16_t current, uint16_t previous, int modulus) {
  int delta = (int)current - (int)previous;
  if (delta > modulus / 2) delta -= modulus;
  if (delta < -modulus / 2) delta += modulus;
  return delta;
}
}  // namespace

int FzeroFfbListDevices(char names[][256], int max_devices) {
  if (!names || max_devices <= 0) return 0;
#ifdef _WIN32
  WheelFfbApi api{};
  if (!WheelFfb_LoadBeside(&api, GetModuleHandleW(nullptr), L"WheelFfb.dll"))
    return 0;
  int found = api.EnumerateDevices();
  int count = 0;
  for (int i = 0; i < found && count < max_devices; ++i) {
    if (api.GetDeviceName(i, names[count], 256) && names[count][0]) ++count;
  }
  WheelFfb_Unload(&api);
  return count;
#else
  return 0;
#endif
}

void FzeroFfbCompute(FzeroFfbState *state, const uint8_t *ram,
                     size_t ram_size, uint32_t input, int strength,
                     FzeroFfbOutput *out) {
  if (!out) return;
  std::memset(out, 0, sizeof(*out));
  if (!state || !ram || ram_size < 0x0be2) return;
  strength = std::max(0, std::min(strength, 100));

  const bool racing = ram[0x54] == 2 && ram[0x55] >= 3;
  const uint16_t x = read16(ram + 0x0b70) & 0x1fff;
  const uint16_t y = read16(ram + 0x0b90) & 0x0fff;
  if (racing && state->have_position) {
    const int dx = wrapped_delta(x, state->previous_x, 0x2000);
    const int dy = wrapped_delta(y, state->previous_y, 0x1000);
    const float instantaneous = std::sqrt((float)(dx * dx + dy * dy));
    state->speed += (instantaneous - state->speed) * 0.25f;
  } else {
    state->speed = 0.0f;
  }
  state->previous_x = x;
  state->previous_y = y;
  state->have_position = racing ? 1 : 0;

  /* $00C9 is the power/energy meter. The verified drive contains isolated
   * 24-unit losses that the former 32-unit threshold missed, alongside four
   * larger contacts (40–240). A 16-unit threshold includes those lighter
   * contacts while excluding the repeated five-unit drain. Boost can spend
   * power too, so it must not become a false collision cue. */
  const uint16_t energy = read16(ram + 0x00c9);
  const int lost_energy = state->have_energy && state->previous_energy > energy ?
      state->previous_energy - energy : 0;
  if (state->collision_cooldown > 0) --state->collision_cooldown;
  out->collision_pulse = racing && state->collision_cooldown == 0 &&
      lost_energy >= kCollisionMinEnergyLoss && !(input & 0x0100u);
  if (out->collision_pulse) state->collision_cooldown = 8;
  state->previous_energy = energy;
  state->have_energy = racing ? 1 : 0;
  out->racing = racing ? 1 : 0;
  if (!racing) return;

  const int direction = (input & 0x0040u) ? 1 : (input & 0x0080u) ? -1 : 0;
  /* A captured active race at ~300 km/h advances the position by about two
   * world units per frame. Dividing by 24 kept the default 35% spring below
   * WheelFfb's 500-unit update step, leaving the wheel effectively limp. */
  const float speed_scale = std::min(state->speed / 2.0f, 1.0f);
  out->constant_force = (int)(direction * strength * 55.0f * speed_scale);
  out->spring_coefficient = (int)(strength * 100.0f * speed_scale);
  out->damper_coefficient = (int)(strength * 40.0f * speed_scale);

  /* Surface bits are non-zero on rough/slip zones. Keep normal track texture
   * subtle and raise it on those zones; frequency follows vehicle speed. */
  const bool rough = ram[0x00c7] != 0;
  out->road_magnitude = (int)(strength * (rough ? 70.0f : 40.0f) * speed_scale);
  out->road_frequency_millihz = 18000 + (int)(speed_scale * 24000.0f);
}

void FzeroFfbInit(const char *config_path, void *native_window) {
  FzeroFfbShutdown();
  int enabled = 0;
  if (!FzeroIniReadInt(config_path, "ForceFeedback", "Enabled", &enabled) ||
      !enabled) return;
  s_strength = 40;
  s_impact_strength = 20;
  FzeroIniReadInt(config_path, "ForceFeedback", "Strength", &s_strength);
  FzeroIniReadInt(config_path, "ForceFeedback", "ImpactStrength", &s_impact_strength);
  s_strength = std::max(0, std::min(s_strength, 100));
  s_impact_strength = std::max(0, std::min(s_impact_strength, 100));

#ifdef _WIN32
  char impact_type[32] = "Constant";
  FzeroIniReadString(config_path, "ForceFeedback", "ImpactType", impact_type,
                     sizeof(impact_type));
  const bool request_constant = std::strcmp(impact_type, "Sine") != 0;
  char requested[256] = "";
  if (!FzeroIniReadString(config_path, "ForceFeedback", "Device", requested,
                          sizeof(requested)) || !requested[0]) {
    std::fprintf(stderr, "[fzero-ffb] Device is required; disabled\n");
    return;
  }
  if (!WheelFfb_LoadBeside(&s_ffb, GetModuleHandleW(nullptr), L"WheelFfb.dll")) {
    std::fprintf(stderr, "[fzero-ffb] WheelFfb.dll unavailable (%s); disabled\n",
                 WheelFfb_MissingExport(&s_ffb));
    WheelFfb_Unload(&s_ffb);
    return;
  }
  s_ffb.SetStrictDeviceSelection(1);
  const int count = s_ffb.EnumerateDevices();
  int match = -1;
  for (int i = 0; i < count; ++i) {
    char name[256] = "";
    if (!s_ffb.GetDeviceName(i, name, sizeof(name))) continue;
    if (_stricmp(name, requested) == 0) {
      if (match >= 0) { match = -2; break; }
      match = i;
    }
  }
  unsigned char guid[16]{};
  if (match < 0 || !s_ffb.GetDeviceGuid(match, guid)) {
    std::fprintf(stderr, "[fzero-ffb] unique device '%s' not found; disabled\n",
                 requested);
    WheelFfb_Unload(&s_ffb);
    return;
  }
  s_ffb.SetPreferredDeviceGuid(guid);
  if (!s_ffb.InitDirectInput((int)(intptr_t)native_window)) {
    std::fprintf(stderr, "[fzero-ffb] could not open '%s'; disabled\n", requested);
    FzeroFfbShutdown();
    return;
  }
  s_ffb.InstallExitGuards();
  if (!s_ffb.SetAutoCenter(0))
    std::fprintf(stderr, "[fzero-ffb] hardware autocenter disable refused; see WheelFfb log\n");
  if (!s_ffb.StartEffect()) {
    std::fprintf(stderr, "[fzero-ffb] constant effect failed (HRESULT %08x); disabled\n",
                 (unsigned)s_ffb.GetLastHResult());
    FzeroFfbShutdown();
    return;
  }
  /* On some bases the first constant-force write loses exclusive access and
   * reacquires it. Do that before creating the spring/damper/road effects:
   * reacquisition can leave already-started effects silent until a pause. */
  if (!s_ffb.SetDeviceForcesXY(0, 0))
    std::fprintf(stderr, "[fzero-ffb] initial zero-force update rejected (HRESULT %08x)\n",
                 (unsigned)s_ffb.GetLastHResult());
  /* SetHoldTimeoutMs starts a worker in WheelFfb.dll. Do not start it until
   * every early-return path has succeeded, or unload would strand the worker
   * executing code from an unloaded module. */
  s_ffb.SetHoldTimeoutMs(250);
  s_spring = s_ffb.CreateConditionEffect(0);
  s_damper = s_ffb.CreateConditionEffect(1);
  s_road = s_ffb.CreatePeriodicEffect(25);
  if (request_constant) {
    s_create_constant_burst = reinterpret_cast<CreateConstantBurstFn>(
        GetProcAddress(s_ffb.module, "CreateConstantBurst"));
    s_play_constant_burst = reinterpret_cast<PlayConstantBurstFn>(
        GetProcAddress(s_ffb.module, "PlayConstantBurst"));
    s_release_constant_bursts = reinterpret_cast<ReleaseConstantBurstsFn>(
        GetProcAddress(s_ffb.module, "ReleaseConstantBursts"));
    if (s_create_constant_burst && s_play_constant_burst && s_release_constant_bursts)
      s_collision_constant = s_create_constant_burst(kConstantImpactMs);
    if (s_collision_constant >= 0) s_use_constant_impact = true;
    else std::fprintf(stderr,
        "[fzero-ffb] constant crash cue unavailable; falling back to sine burst\n");
  }
  if (!s_use_constant_impact)
    s_collision_sine = s_ffb.CreatePeriodicBurst(32, kSineImpactMs);
  s_active = true;
  s_trace_frames = 0;
  s_trace_enabled = std::getenv("FZERO_FFB_TRACE") != nullptr;
  std::fprintf(stderr, "[fzero-ffb] active on %s at %d%% (spring=%d damper=%d road=%d impact=%s/%d/%d%%)\n",
               requested, s_strength, s_spring, s_damper, s_road,
               s_use_constant_impact ? "constant" : "sine",
               s_use_constant_impact ? s_collision_constant : s_collision_sine,
               s_impact_strength);
#else
  (void)native_window;
  std::fprintf(stderr, "[fzero-ffb] unavailable on this platform\n");
#endif
}

void FzeroFfbFrame(const uint8_t *ram, size_t ram_size, uint32_t input) {
#ifdef _WIN32
  if (!s_active) return;
  FzeroFfbOutput output{};
  FzeroFfbCompute(&s_state, ram, ram_size, input, s_strength, &output);
  /* The SNES input is pulse-density modulated, so applying those digital
   * pulses directly to constant force feels like a brief tick followed by
   * silence. Let the wheel's own position-sensitive spring hold a continuous
   * centering load; retain the digital force only when spring is unsupported. */
  if (!s_ffb.SetDeviceForcesXY(s_spring >= 0 ? 0 : output.constant_force, 0))
    std::fprintf(stderr, "[fzero-ffb] force update rejected (HRESULT %08x)\n",
                 (unsigned)s_ffb.GetLastHResult());
  int spring_ok = s_spring < 0 ? -1 : s_ffb.UpdateConditionEffect(
      s_spring, output.spring_coefficient, 10000, 0, 0);
  int damper_ok = s_damper < 0 ? -1 : s_ffb.UpdateConditionEffect(
      s_damper, output.damper_coefficient, 10000, 0, 0);
  int road_ok = s_road < 0 ? -1 : s_ffb.UpdatePeriodicEffect(
      s_road, output.road_magnitude, output.road_frequency_millihz);
  if (s_trace_enabled && (++s_trace_frames % 180 == 0 ||
                          (output.racing && s_trace_frames < 181)))
    std::fprintf(stderr, "[fzero-ffb-trace] frame=%u racing=%d speed=%.2f "
                 "spring=%d/%d damper=%d/%d road=%d/%d hr=%08x\n",
                 s_trace_frames, output.racing, s_state.speed,
                 output.spring_coefficient, spring_ok,
                 output.damper_coefficient, damper_ok,
                 output.road_magnitude, road_ok,
                 (unsigned)s_ffb.GetLastHResult());
  if (output.collision_pulse) {
    const int magnitude = s_impact_strength * 100;
    const int accepted = s_use_constant_impact ?
        (s_collision_constant >= 0 && s_play_constant_burst ?
            s_play_constant_burst(s_collision_constant, magnitude) : 0) :
        (s_collision_sine >= 0 ?
            s_ffb.PlayPeriodicBurst(s_collision_sine, magnitude, 32000) : 0);
    if (s_trace_enabled)
      std::fprintf(stderr, "[fzero-ffb-impact] frame=%u energy=%u type=%s "
                   "magnitude=%d duration_ms=%d accepted=%d slot=%d hr=%08x\n", s_trace_frames,
                   (unsigned)(ram[0xc9] | ((unsigned)ram[0xca] << 8)),
                   s_use_constant_impact ? "constant" : "sine", magnitude,
                   s_use_constant_impact ? kConstantImpactMs : kSineImpactMs,
                   accepted, s_use_constant_impact ? s_collision_constant : s_collision_sine,
                   (unsigned)s_ffb.GetLastHResult());
  }
#else
  (void)ram; (void)ram_size; (void)input;
#endif
}

void FzeroFfbSilence(void) {
#ifdef _WIN32
  if (s_active) s_ffb.ZeroForces();
#endif
}

void FzeroFfbShutdown(void) {
#ifdef _WIN32
  if (s_ffb.module) {
    s_ffb.PanicStop();
    if (s_release_constant_bursts) s_release_constant_bursts();
    s_ffb.ReleasePeriodicEffects();
    s_ffb.ReleaseConditionEffects();
    s_ffb.FreeDirectInput();
    WheelFfb_Unload(&s_ffb);
  }
  s_spring = s_damper = s_road = s_collision_sine = s_collision_constant = -1;
  s_use_constant_impact = false;
  s_create_constant_burst = nullptr;
  s_play_constant_burst = nullptr;
  s_release_constant_bursts = nullptr;
  s_active = false;
  s_trace_enabled = false;
#endif
  std::memset(&s_state, 0, sizeof(s_state));
}
