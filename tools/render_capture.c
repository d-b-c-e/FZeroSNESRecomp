#include "fzero_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FzeroViewport g_viewport;
/* Presentation blend for the sequence mode. The desktop host presents between
 * simulations, so 1 alone never exercises the interpolated path. */
static double g_alpha = 1;

static int write_ppm(const uint32_t *output, const char *path, bool report) {
  FzeroViewport v = g_viewport;
  FILE *f = fopen(path, "wb");
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
    if (row_differences && report) fprintf(stderr, "row %d: %u differing pixels\n", y, row_differences);
    differences += row_differences;
  }
  if (fclose(f)) return 4;
  if (report) {
    if (v.width == 256) fprintf(stderr, "native capture: width=256 stock_diff_pixels=%u\n", differences);
    else fprintf(stderr, "native capture: width=%d (wide output, no stock comparison)\n", v.width);
  }
  return v.width == 256 && differences ? 1 : 0;
}

/* One capture through the compositor. The renderer carries state between
 * frames - interpolation sources and the streamed Mode 7 course record - so a
 * sequence has to be replayed in order to represent what a player sees. */
static int render_one(const char *capture, const char *output, bool report) {
  if (!FzeroRendererLoadCapture(capture)) return 2;
  static uint32_t pixels[FZERO_MAX_WIDTH * 224];
  if (!FzeroRendererDraw(pixels, g_viewport, g_alpha)) return 3;
  return write_ppm(pixels, output, report);
}

int main(int argc, char **argv) {
  bool sequence = argc > 1 && strncmp(argv[1], "--sequence", 10) == 0;
  if (sequence && argv[1][10] == '=') g_alpha = atof(argv[1] + 11);
  else if (sequence && argv[1][10]) sequence = false;
  if (sequence ? argc < 5 : argc != 4) {
    fputs("usage: FZeroRenderCapture capture.bin aspect output.ppm\n"
          "       FZeroRenderCapture --sequence aspect output-directory capture.bin...\n",
          stderr);
    return 2;
  }
  FzeroVideoSettings settings;
  FzeroVideoStock(&settings);
  settings.enhanced = true;
  if (!FzeroParseAspect(argv[2], &settings.aspect)) return 2;
  g_viewport = FzeroCalculateViewport(&settings, 1920, 1080);
  if (!sequence) return render_one(argv[1], argv[3], true);
  for (int i = 4; i < argc; ++i) {
    char path[1024];
    const char *base = argv[i];
    for (const char *p = argv[i]; *p; ++p)
      if (*p == '/' || *p == '\\') base = p + 1;
    size_t length = strlen(base);
    if (length > 4 && strcmp(base + length - 4, ".bin") == 0) length -= 4;
    if (snprintf(path, sizeof(path), "%s/%.*s.ppm", argv[3], (int)length, base) >=
        (int)sizeof(path)) return 2;
    int status = render_one(argv[i], path, false);
    if (status) return status;
  }
  fprintf(stderr, "native capture sequence: %d frames at width=%d\n",
          argc - 4, g_viewport.width);
  return 0;
}
