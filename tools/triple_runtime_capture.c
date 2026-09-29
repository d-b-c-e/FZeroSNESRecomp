/* Render the runtime side-panel API from an immutable private race capture.
 * The two-panel PPM can be compared with FZeroTripleGroundCapture's sides. */
#include "fzero_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    fprintf(stderr, "usage: FZeroTripleRuntimeCapture capture.bin sides.ppm [iterations]\n");
    return 2;
  }
  if (!FzeroRendererLoadCapture(argv[1])) {
    fprintf(stderr, "invalid capture\n");
    return 2;
  }
  enum { pw = 640, ph = 360, span = 2 * pw };
  FzeroTripleRig rig = {708.4166, 398.4843, 660, 0, 70, 70, 8, pw, ph};
  FzeroVideoSettings video;
  FzeroVideoDefaults(&video);
  int logical_width = FzeroCalculateViewport(&video, 2560, 1440).width;
  uint32_t *buffers[2] = {
      calloc((size_t)span * ph, sizeof(uint32_t)),
      calloc((size_t)span * ph, sizeof(uint32_t))};
  if (!buffers[0] || !buffers[1]) {
    free(buffers[0]); free(buffers[1]);
    return 3;
  }
  int iterations = argc == 4 ? atoi(argv[3]) : 1;
  if (iterations < 1 || iterations > 10000) {
    free(buffers[0]); free(buffers[1]);
    return 2;
  }
  clock_t start = clock();
  for (int i = 0; i < iterations; ++i)
    /* Alternate destinations so the same-frame output cache cannot turn a
     * multi-iteration benchmark into one render plus no-op calls. */
    if (!FzeroRendererDrawTripleSides(buffers[i & 1], (size_t)span * ph,
                                     &rig, logical_width)) {
    fprintf(stderr, "runtime side projection rejected capture\n");
    free(buffers[0]); free(buffers[1]);
    return 3;
    }
  fprintf(stderr, "side_compositor_cpu_ms_per_frame=%.3f (%d iterations)\n",
          1000.0 * (clock() - start) / CLOCKS_PER_SEC / iterations, iterations);
  uint32_t *pixels = buffers[(iterations - 1) & 1];
  FILE *out = fopen(argv[2], "wb");
  if (!out) { free(buffers[0]); free(buffers[1]); return 3; }
  fprintf(out, "P6\n%d %d\n255\n", span, ph);
  for (int y = 0; y < ph; ++y) {
    for (int side = 0; side < 2; ++side) {
      for (int x = 0; x < pw; ++x) {
        uint32_t pixel = pixels[(size_t)side * pw * ph + (size_t)y * pw + x];
        unsigned char rgb[3] = {(unsigned char)(pixel >> 16),
                                (unsigned char)(pixel >> 8),
                                (unsigned char)pixel};
        if (fwrite(rgb, sizeof(rgb), 1, out) != 1) {
          fclose(out); free(buffers[0]); free(buffers[1]); return 3;
        }
      }
    }
  }
  int result = fclose(out);
  free(buffers[0]); free(buffers[1]);
  return result ? 3 : 0;
}
