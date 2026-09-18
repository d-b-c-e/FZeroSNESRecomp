#include "fzero_mode7.h"

#include <math.h>

static int sign13(int x) {
  x &= 0x1fff;
  return x & 0x1000 ? x - 0x2000 : x;
}
static int clip10(int x) {
  return x & 0x2000 ? (x | ~1023) : (x & 1023);
}

FzeroMode7Line FzeroMode7Transform(const int16_t m[8], uint8_t control,
                                   unsigned scanline) {
  int cx = sign13(m[4]), cy = sign13(m[5]);
  int h = clip10(sign13(m[6]) - cx), v = clip10(sign13(m[7]) - cy);
  int y = control & 2 ? 255 - (int)scanline : (int)scanline;
  double ox = (m[0] * h & ~63) + (m[1] * y & ~63) + (m[1] * v & ~63) + cx * 256;
  double oy = (m[2] * h & ~63) + (m[3] * y & ~63) + (m[3] * v & ~63) + cy * 256;
  double dx = m[0], dy = m[2];
  if (control & 1) {
    ox += dx * 255; oy += dy * 255;
    dx = -dx; dy = -dy;
  }
  return (FzeroMode7Line){ox, oy, dx, dy, control};
}

FzeroMode7Texel FzeroMode7Locate(const FzeroMode7Line *line, double x) {
  return (FzeroMode7Texel){floor((line->origin_x + x * line->step_x) / 256),
                           floor((line->origin_y + x * line->step_y) / 256)};
}

uint8_t FzeroMode7Fetch(const FzeroMode7Line *line, const uint16_t vram[0x8000],
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

uint8_t FzeroMode7Sample(const FzeroMode7Line *line,
                         const uint16_t vram[0x8000], double x) {
  return FzeroMode7Fetch(line, vram, FzeroMode7Locate(line, x), -1);
}

static double periodic_delta(double from, double to) {
  return remainder(to - from, 1024.0 * 256.0);
}
double FzeroMode7Blend(const FzeroMode7Line *a, const FzeroMode7Line *b,
                       double alpha) {
  if (!isfinite(alpha) || alpha >= 1 || a->control != b->control) return 1;
  if (alpha <= 0) return 0;
  return alpha;
}

FzeroMode7Line FzeroMode7Interpolate(FzeroMode7Line a, FzeroMode7Line b,
                                     double alpha) {
  if (!isfinite(alpha) || alpha >= 1 || a.control != b.control) return b;
  if (alpha <= 0) return a;
  FzeroMode7Line r = b;
  /* Large-field overflow modes have real edges, so do not wrap their origins. */
  r.origin_x = a.origin_x + alpha * ((b.control & 0x80) ?
      b.origin_x - a.origin_x : periodic_delta(a.origin_x, b.origin_x));
  r.origin_y = a.origin_y + alpha * ((b.control & 0x80) ?
      b.origin_y - a.origin_y : periodic_delta(a.origin_y, b.origin_y));
  r.step_x = a.step_x + alpha * (b.step_x - a.step_x);
  r.step_y = a.step_y + alpha * (b.step_y - a.step_y);
  return r;
}

bool FzeroMode7Project(const FzeroMode7Line *line, double wx, double wy,
                        double *sx, double *residual) {
  double norm = line->step_x * line->step_x + line->step_y * line->step_y;
  if (!isfinite(norm) || norm < 1e-12 || !isfinite(wx) || !isfinite(wy)) return false;
  double dx = wx * 256 - line->origin_x, dy = wy * 256 - line->origin_y;
  *sx = (dx * line->step_x + dy * line->step_y) / norm;
  *residual = fabs(dx * line->step_y - dy * line->step_x) / sqrt(norm) / 256;
  return isfinite(*sx) && isfinite(*residual);
}
