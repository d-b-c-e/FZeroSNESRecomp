#include "fzero_analog.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static int count_pulses(FzeroAnalogSteering *state, int axis, int deadzone,
                        int frames, uint32_t expected_bit) {
  int count = 0;
  for (int i = 0; i < frames; ++i) {
    uint32_t result = FzeroAnalogSteeringRead(state, axis, deadzone);
    CHECK(result == 0 || result == expected_bit);
    if (result) ++count;
  }
  return count;
}

int main(void) {
  FzeroAnalogSteering state = {0};

  CHECK(count_pulses(&state, 32767, 0, 120, 0x80) == 120);
  FzeroAnalogSteeringReset(&state);
  CHECK(count_pulses(&state, -32768, 0, 120, 0x40) == 120);

  FzeroAnalogSteeringReset(&state);
  int half = count_pulses(&state, 16384, 0, 120, 0x80);
  CHECK(half >= 59 && half <= 61);

  FzeroAnalogSteeringReset(&state);
  CHECK(count_pulses(&state, 3276, 3276, 120, 0x80) == 0);
  CHECK(count_pulses(&state, -3276, 3276, 120, 0x40) == 0);

  /* A direction change cannot carry phase from the previous direction. */
  FzeroAnalogSteeringReset(&state);
  CHECK(FzeroAnalogSteeringRead(&state, 12000, 0) == 0);
  CHECK(FzeroAnalogSteeringRead(&state, -12000, 0) == 0);
  CHECK(state.direction == -1);

  /* Returning to centre clears phase, preventing a delayed input pulse. */
  FzeroAnalogSteeringReset(&state);
  CHECK(FzeroAnalogSteeringRead(&state, 12000, 0) == 0);
  CHECK(FzeroAnalogSteeringRead(&state, 0, 0) == 0);
  CHECK(FzeroAnalogSteeringRead(&state, 12000, 0) == 0);

  puts("F-Zero analog steering pulse-density tests passed");
  return 0;
}
