#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FzeroFfbState {
  uint16_t previous_x, previous_y;
  float speed;
  int have_position;
  uint16_t previous_energy;
  int have_energy;
  int collision_cooldown;
} FzeroFfbState;

typedef struct FzeroFfbOutput {
  int constant_force;
  int road_magnitude;
  int road_frequency_millihz;
  int spring_coefficient;
  int damper_coefficient;
  int collision_pulse;
  int racing;
} FzeroFfbOutput;

void FzeroFfbCompute(FzeroFfbState *state, const uint8_t *ram,
                     size_t ram_size, uint32_t input, int strength,
                     FzeroFfbOutput *out);
void FzeroFfbInit(const char *config_path, void *native_window);
void FzeroFfbFrame(const uint8_t *ram, size_t ram_size, uint32_t input);
void FzeroFfbShutdown(void);
void FzeroFfbSilence(void);
/* Exact DirectInput device names available for force feedback. */
int FzeroFfbListDevices(char names[][256], int max_devices);

#ifdef __cplusplus
}
#endif
