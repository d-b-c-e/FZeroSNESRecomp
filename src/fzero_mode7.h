#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Renderer-owned scanline transform, in Q8 texture coordinates. No pointers
 * into mutable guest memory: callers latch registers and VRAM at scanout. */
typedef struct FzeroMode7Line {
  double origin_x, origin_y, step_x, step_y;
  uint8_t control;
} FzeroMode7Line;

FzeroMode7Line FzeroMode7Transform(const int16_t matrix[8], uint8_t control,
                                   unsigned scanline);
/* Returns a palette index; zero is transparent. X is a signed logical SNES
 * coordinate and may extend beyond either stock screen edge. */
uint8_t FzeroMode7Sample(const FzeroMode7Line *line,
                         const uint16_t vram[0x8000], double x);
/* Eligibility (scene, camera discontinuities, loads) belongs to the snapshot
 * publisher. Wrap periodic texture coordinates, never linearly blend pixels. */
FzeroMode7Line FzeroMode7Interpolate(FzeroMode7Line previous,
                                     FzeroMode7Line current, double alpha);
/* Solve this scanline's world-to-screen transform. Residual is measured in
 * texture pixels and lets the object projector find the matching scanline. */
bool FzeroMode7Project(const FzeroMode7Line *line, double world_x,
                        double world_y, double *screen_x, double *residual);
