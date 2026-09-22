#include "fzero_ffb.h"

extern "C" {
#include "fzero_hotkeys.h"
}

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#include "../lib/toolkit/include/wheelffb.h"
#endif

namespace {
FzeroFfbState s_state{};
int s_strength = 40;

#ifdef _WIN32
WheelFfbApi s_ffb{};
int s_damper = -1;
int s_road = -1;
int s_collision = -1;
bool s_active = false;
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

  const bool collision = racing &&
      (ram[0x00e0] != 0 || ram[0x00e8] != 0 || ram[0x00e9] != 0 ||
       ram[0x00f5] != 0);
  out->collision_pulse = collision && !state->collision_active;
  state->collision_active = collision ? 1 : 0;
  out->racing = racing ? 1 : 0;
  if (!racing) return;

  const int direction = (input & 0x0040u) ? 1 : (input & 0x0080u) ? -1 : 0;
  const float speed_scale = std::min(state->speed / 24.0f, 1.0f);
  out->constant_force = (int)(direction * strength * 55.0f * speed_scale);

  /* Surface bits are non-zero on rough/slip zones. Keep normal track texture
   * subtle and raise it on those zones; frequency follows vehicle speed. */
  const bool rough = ram[0x00c7] != 0;
  out->road_magnitude = (int)(strength * (rough ? 38.0f : 10.0f) * speed_scale);
  out->road_frequency_millihz = 18000 + (int)(speed_scale * 24000.0f);
}

void FzeroFfbInit(const char *config_path, void *native_window) {
  FzeroFfbShutdown();
  int enabled = 0;
  if (!FzeroIniReadInt(config_path, "ForceFeedback", "Enabled", &enabled) ||
      !enabled) return;
  FzeroIniReadInt(config_path, "ForceFeedback", "Strength", &s_strength);
  s_strength = std::max(0, std::min(s_strength, 100));

#ifdef _WIN32
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
  s_ffb.SetHoldTimeoutMs(250);
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
    WheelFfb_Unload(&s_ffb);
    return;
  }
  s_ffb.InstallExitGuards();
  s_ffb.SetAutoCenter(0);
  s_ffb.StartEffect();
  s_damper = s_ffb.CreateConditionEffect(1);
  if (s_damper >= 0)
    s_ffb.UpdateConditionEffect(s_damper, 1200, 10000, 0, 0);
  s_road = s_ffb.CreatePeriodicEffect(25);
  s_collision = s_ffb.CreatePeriodicBurst(32, 140);
  s_active = true;
  std::fprintf(stderr, "[fzero-ffb] active on %s at %d%%\n", requested,
               s_strength);
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
  s_ffb.SetDeviceForcesXY(output.constant_force, 0);
  if (s_road >= 0)
    s_ffb.UpdatePeriodicEffect(s_road, output.road_magnitude,
                               output.road_frequency_millihz);
  if (output.collision_pulse && s_collision >= 0)
    s_ffb.PlayPeriodicBurst(s_collision, s_strength * 85, 32000);
#else
  (void)ram; (void)ram_size; (void)input;
#endif
}

void FzeroFfbShutdown(void) {
#ifdef _WIN32
  if (s_ffb.module) {
    s_ffb.PanicStop();
    s_ffb.ReleasePeriodicEffects();
    s_ffb.ReleaseConditionEffects();
    s_ffb.FreeDirectInput();
    WheelFfb_Unload(&s_ffb);
  }
  s_damper = s_road = s_collision = -1;
  s_active = false;
#endif
  std::memset(&s_state, 0, sizeof(s_state));
}
