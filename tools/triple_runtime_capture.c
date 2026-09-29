/* Render the runtime side-panel API from an immutable private race capture.
 * The two-panel PPM can be compared with FZeroTripleGroundCapture's sides. */
#include "fzero_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: FZeroTripleRuntimeCapture capture.bin sides.ppm [iterations] [--runtime] [--vehicles] [--vehicle-preview]\n"
                    "   or: FZeroTripleRuntimeCapture capture.bin --vehicles [--runtime]\n");
    return 2;
  }
  bool vehicles_only = !strcmp(argv[2], "--vehicles");
  bool show_vehicles = vehicles_only, runtime_size = false;
  bool vehicle_preview = false;
  int iterations = 1;
  bool have_iterations = false;
  for (int i = 3; i < argc; ++i) {
    if (!strcmp(argv[i], "--runtime")) {
      if (runtime_size) return 2;
      runtime_size = true;
    } else if (!strcmp(argv[i], "--vehicles") && !vehicles_only) {
      if (show_vehicles) return 2;
      show_vehicles = true;
    } else if (!strcmp(argv[i], "--vehicle-preview") && !vehicles_only) {
      if (vehicle_preview) return 2;
      vehicle_preview = true;
    } else if (!vehicles_only && !have_iterations) {
      char *end;
      long value = strtol(argv[i], &end, 10);
      if (!argv[i][0] || *end || value < 1 || value > 10000) return 2;
      iterations = (int)value;
      have_iterations = true;
    } else {
      return 2;
    }
  }
  if (!FzeroRendererLoadCapture(argv[1])) {
    fprintf(stderr, "invalid capture\n");
    return 2;
  }
  /* The 640x360 default retains the high-resolution comparison fixture;
   * --runtime matches the 512x288 side textures used in the SDL presenter. */
  int pw = runtime_size ? 512 : 640;
  int ph = runtime_size ? 288 : 360;
  int span = 2 * pw;
  FzeroTripleRig rig = {708.4166, 398.4843, 660, 0, 70, 70, 8, pw, ph};
  FzeroVideoSettings video;
  FzeroVideoDefaults(&video);
  int logical_width = FzeroCalculateViewport(&video, 2560, 1440).width;
  if (show_vehicles) {
    FzeroTripleVehicleProbe probes[6];
    if (!FzeroRendererProbeTripleVehicles(&rig, logical_width, probes)) {
      fprintf(stderr, "vehicle probe rejected capture\n");
      return 3;
    }
    puts("car,state,world_x,world_y,guest_x,guest_y,oam_slots,raster_sprite_pixels,raster_left,raster_top,raster_right,raster_bottom,panel,x,y,on_panel");
    for (int car = 0; car < 6; ++car)
      for (int side = 0; side < 3; ++side) {
        const FzeroTripleVehicleProbe *probe = &probes[car];
        double x = probe->panel_x[side], y = probe->panel_y[side];
        int on_panel = probe->projected[side] &&
            x >= 0 && x < pw && y >= 0 && y < ph;
        printf("%d,%02x,%d,%d,%d,%d,%u,%u,%d,%d,%d,%d,%d,%.2f,%.2f,%d\n",
            car, probe->state, probe->world_x, probe->world_y,
            probe->guest_x, probe->guest_y, probe->oam_slots,
            probe->raster_sprite_pixels,
            probe->raster_left, probe->raster_top,
            probe->raster_right, probe->raster_bottom,
            side, x, y, on_panel);
      }
  }
  if (vehicles_only) return 0;
  uint32_t *buffers[2] = {
      calloc((size_t)span * ph, sizeof(uint32_t)),
      calloc((size_t)span * ph, sizeof(uint32_t))};
  if (!buffers[0] || !buffers[1]) {
    free(buffers[0]); free(buffers[1]);
    return 3;
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
  if (vehicle_preview) {
    unsigned written = 0;
    if (!FzeroRendererPreviewTripleVehicles(pixels, (size_t)span * ph,
                                            &rig, logical_width, &written)) {
      fprintf(stderr, "vehicle preview rejected capture\n");
      free(buffers[0]); free(buffers[1]); return 3;
    }
    fprintf(stderr, "vehicle_preview_pixels=%u (offline only)\n", written);
  }
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
