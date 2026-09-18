#pragma once

#include <math.h>
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
/* Unwrapped texture coordinates for one sample. The Mode 7 map repeats every
 * 1024 units, so only the raw value carries the sample's offset from this
 * scanline's rotation centre; the wrapped coordinate alone is ambiguous.
 *
 * Both of these run once per output pixel, so they are defined here: a call
 * across the translation unit boundary costs more than the work they do. */
typedef struct FzeroMode7Texel { double x, y; } FzeroMode7Texel;

static inline FzeroMode7Texel FzeroMode7Locate(const FzeroMode7Line *line, double x) {
  FzeroMode7Texel texel = {floor((line->origin_x + x * line->step_x) / 256),
                           floor((line->origin_y + x * line->step_y) / 256)};
  return texel;
}

/* Read one texel. A non-negative `tile` replaces the tilemap lookup, so a
 * caller holding a course map of its own can address character data the
 * current tilemap no longer describes. Character data is read from VRAM. */
static inline uint8_t FzeroMode7Fetch(const FzeroMode7Line *line,
                                      const uint16_t vram[0x8000],
                                      FzeroMode7Texel texel, int tile) {
  double qx = texel.x, qy = texel.y;
  if (!isfinite(qx) || !isfinite(qy)) return 0;
  bool outside = qx < 0 || qx >= 1024 || qy < 0 || qy >= 1024;
  if (outside && (line->control & 0x80) && !(line->control & 0x40)) return 0;
  /* Reduce before conversion so even an invalid large transform cannot
   * overflow the integer conversion or escape the immutable VRAM snapshot. */
  int tx = (int)fmod(qx, 1024), ty = (int)fmod(qy, 1024);
  tx = (tx + 1024) & 1023; ty = (ty + 1024) & 1023;
  unsigned number = outside && (line->control & 0x80) ? 0 :
      tile >= 0 ? (unsigned)tile & 255 : vram[(ty / 8) * 128 + tx / 8] & 255;
  return vram[number * 64 + (ty & 7) * 8 + (tx & 7)] >> 8;
}
/* Returns a palette index; zero is transparent. X is a signed logical SNES
 * coordinate and may extend beyond either stock screen edge. */
uint8_t FzeroMode7Sample(const FzeroMode7Line *line,
                         const uint16_t vram[0x8000], double x);
/* Eligibility (scene, camera discontinuities, loads) belongs to the snapshot
 * publisher. Wrap periodic texture coordinates, never linearly blend pixels. */
FzeroMode7Line FzeroMode7Interpolate(FzeroMode7Line previous,
                                     FzeroMode7Line current, double alpha);
/* The factor FzeroMode7Interpolate will actually apply: 1 where it keeps the
 * current line unchanged and 0 where it keeps the previous one. Anything a
 * caller measures a blended texel against has to use the same factor. */
double FzeroMode7Blend(const FzeroMode7Line *previous,
                       const FzeroMode7Line *current, double alpha);
/* Solve this scanline's world-to-screen transform. Residual is measured in
 * texture pixels and lets the object projector find the matching scanline. */
bool FzeroMode7Project(const FzeroMode7Line *line, double world_x,
                        double world_y, double *screen_x, double *residual);
