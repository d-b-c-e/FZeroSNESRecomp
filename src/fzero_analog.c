#include "fzero_analog.h"

#include <stdlib.h>
#include <math.h>

enum {
  FZERO_INPUT_LEFT = 0x0040u,
  FZERO_INPUT_RIGHT = 0x0080u,
  FZERO_ANALOG_ONE = 0x10000u,
};

void FzeroAnalogSteeringReset(FzeroAnalogSteering *state) {
  if (!state) return;
  state->phase = 0;
  state->direction = 0;
}

uint32_t FzeroAnalogSteeringRead(FzeroAnalogSteering *state, int axis,
                                 int deadzone) {
  return FzeroAnalogSteeringReadTuned(state, axis, deadzone, 100, 100);
}

uint32_t FzeroAnalogSteeringReadTuned(FzeroAnalogSteering *state, int axis,
                                     int deadzone, int range_percent,
                                     int response_percent) {
  if (!state) return 0;
  if (deadzone < 0) deadzone = 0;
  if (deadzone > 32766) deadzone = 32766;

  int direction = axis < -deadzone ? -1 : axis > deadzone ? 1 : 0;
  if (!direction) {
    FzeroAnalogSteeringReset(state);
    return 0;
  }
  if (direction != state->direction) {
    state->phase = 0;
    state->direction = direction;
  }

  /* Remove the dead zone, then map the remaining travel to [0, 1].  Use
   * 32768 for negative full lock and 32767 for positive full lock so either
   * direction reaches a true 100% duty cycle. */
  int magnitude = axis < 0 ? -axis : axis;
  int maximum = axis < 0 ? 32768 : 32767;
  if (range_percent < 10) range_percent = 10;
  if (range_percent > 100) range_percent = 100;
  if (response_percent < 25) response_percent = 25;
  if (response_percent > 200) response_percent = 200;
  int span = (int)(((int64_t)(maximum - deadzone) * range_percent) / 100);
  if (span < 1) span = 1;
  double travel = (double)(magnitude - deadzone) / span;
  if (travel > 1.0) travel = 1.0;
  double curved = pow(travel, response_percent / 100.0);
  uint32_t level = (uint32_t)(curved * FZERO_ANALOG_ONE + 0.5);
  if (level >= FZERO_ANALOG_ONE) {
    state->phase = 0;
    return direction < 0 ? FZERO_INPUT_LEFT : FZERO_INPUT_RIGHT;
  }

  state->phase += level;
  if (state->phase < FZERO_ANALOG_ONE) return 0;
  state->phase -= FZERO_ANALOG_ONE;
  return direction < 0 ? FZERO_INPUT_LEFT : FZERO_INPUT_RIGHT;
}
