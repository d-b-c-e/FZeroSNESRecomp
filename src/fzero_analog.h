#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * A SNES controller has no analog steering value.  Preserve proportional
 * wheel input by pulse-density modulating the digital Left/Right button over
 * successive simulation frames.  Full lock is held every frame; smaller
 * deflections produce evenly distributed button presses.
 *
 * This state belongs to one controller and must be reset when the device is
 * disconnected or its steering mode changes.
 */
typedef struct FzeroAnalogSteering {
  uint32_t phase;
  int direction;
} FzeroAnalogSteering;

void FzeroAnalogSteeringReset(FzeroAnalogSteering *state);

/* axis is SDL's signed -32768..32767 range; deadzone uses the same units.
 * Returns SNES joypad bit 6 (Left), bit 7 (Right), or zero. */
uint32_t FzeroAnalogSteeringRead(FzeroAnalogSteering *state, int axis,
                                 int deadzone);
/* range_percent is the fraction of physical axis travel that reaches full
 * lock; response_percent is the curve exponent in percent (50 = sqrt).
 * Both are deliberately independent of the neutral deadzone. */
uint32_t FzeroAnalogSteeringReadTuned(FzeroAnalogSteering *state, int axis,
                                     int deadzone, int range_percent,
                                     int response_percent);
