#include "fzero_renderer.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 4) {
    fputs("usage: FZeroRenderCapture capture.bin aspect output.ppm\n", stderr);
    return 2;
  }
  FzeroVideoSettings settings;
  FzeroVideoDefaults(&settings);
  settings.enhanced = true;
  if (!FzeroParseAspect(argv[2], &settings.aspect) || !FzeroRendererLoadCapture(argv[1])) return 2;
  FzeroViewport v = FzeroCalculateViewport(&settings, 1920, 1080);
  static uint32_t output[FZERO_MAX_WIDTH * 224];
  if (!FzeroRendererDraw(output, v, 1)) return 3;
  FILE *f = fopen(argv[3], "wb");
  if (!f) return 4;
  fprintf(f, "P6\n%d 224\n255\n", v.width);
  unsigned differences = 0;
  for (int y = 0; y < 224; ++y) {
    unsigned row_differences = 0;
    for (int x = 0; x < v.width; ++x) {
      uint32_t p = output[y * v.width + x];
      unsigned char rgb[3] = {(unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p};
      if (fwrite(rgb, 3, 1, f) != 1) { fclose(f); return 4; }
      if (v.width == 256 && p != FzeroRendererStockFrame()[y * 256 + x]) ++row_differences;
    }
    if (row_differences) fprintf(stderr, "row %d: %u differing pixels\n", y, row_differences);
    differences += row_differences;
  }
  if (fclose(f)) return 4;
  if (v.width == 256) fprintf(stderr, "native capture: width=256 stock_diff_pixels=%u\n", differences);
  else fprintf(stderr, "native capture: width=%d (wide output, no stock comparison)\n", v.width);
  return v.width == 256 && differences ? 1 : 0;
}
