#include "fzero_mode7.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
  static uint16_t vram[0x8000];
  /* Every map cell selects tile 1; each texel encodes its column and row.
   * Tile 0 has a separate value for overflow-fill assertions. */
  for (int i = 0; i < 0x4000; ++i) vram[i] = 1;
  for (int i = 0; i < 64; ++i) {
    vram[i] |= 200 << 8;
    vram[64 + i] |= (i + 1) << 8;
  }
  int16_t m[8] = {256, 0, 0, 256, 0, 0, 0, 0};
  FzeroMode7Line line = FzeroMode7Transform(m, 0, 3);
  /* Both margins sample real map texels, including negative coordinates. */
  for (int x = -213; x < 469; ++x)
    CHECK(FzeroMode7Sample(&line, vram, x) == 25 + ((x % 8 + 8) % 8));
  line = FzeroMode7Transform(m, 1, 3);
  CHECK(FzeroMode7Sample(&line, vram, 0) == 32);
  CHECK(FzeroMode7Sample(&line, vram, -1) == 25);
  line = FzeroMode7Transform(m, 2, 3);
  CHECK(FzeroMode7Sample(&line, vram, 0) == 33); /* y=252 */
  line = FzeroMode7Transform(m, 0x80, 3);
  CHECK(FzeroMode7Sample(&line, vram, -1) == 0);
  CHECK(FzeroMode7Sample(&line, vram, 1024) == 0);
  line = FzeroMode7Transform(m, 0xc0, 3);
  CHECK(FzeroMode7Sample(&line, vram, -1) == 200);
  CHECK(FzeroMode7Sample(&line, vram, 0) == 25);
  /* Quarter-turn camera: screen x changes world y; scanline changes world x. */
  m[0] = 0; m[1] = -256; m[2] = 256; m[3] = 0;
  line = FzeroMode7Transform(m, 0, 3);
  CHECK(FzeroMode7Sample(&line, vram, 2) == 22);
  double sx, residual;
  CHECK(FzeroMode7Project(&line, -3, 300, &sx, &residual));
  CHECK(sx == 300 && residual == 0);
  CHECK(FzeroMode7Project(&line, 1, 300, &sx, &residual));
  CHECK(residual == 4);
  FzeroMode7Line a = {1023 * 256, 0, 256, 0, 0}, b = {1 * 256, 0, 256, 0, 0};
  line = FzeroMode7Interpolate(a, b, 0.5);
  CHECK(line.origin_x == 1024 * 256); /* crosses seam by 2 pixels, not 1022 */
  CHECK(FzeroMode7Sample(&line, vram, 0) == 1);
  a.control = b.control = 0x80;
  CHECK(FzeroMode7Interpolate(a, b, 0.5).origin_x == 512 * 256);
  a.control = 0;
  CHECK(FzeroMode7Interpolate(a, b, 0.1).origin_x == b.origin_x);
  line = (FzeroMode7Line){0};
  CHECK(!FzeroMode7Project(&line, 1, 1, &sx, &residual));
  line.origin_x = NAN;
  CHECK(FzeroMode7Sample(&line, vram, 0) == 0);
  puts("F-Zero Mode 7: signed margins, flips, overflow, projection, and interpolation passed");
  return 0;
}
