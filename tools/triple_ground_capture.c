/* OFFLINE EXPERIMENT ONLY: three physical Mode 7 ground projections from one
 * immutable race capture. No sprites, HUD, sky, or live game presentation.
 * Include the existing renderer in this translation unit to reuse its exact
 * capture layout, course resolver, VRAM fetch, and colour math without adding
 * dependencies or calls to the shipping host. */
#if defined(_MSC_VER)
/* The pinned runner uses one GCC alignment spelling in ppu.h. This offline
 * MSVC tool does not instantiate a PPU, so discard that declaration-only
 * attribute here rather than editing the external submodule to build it. */
#define __attribute__(x)
#endif
#include "../src/fzero_renderer.c"
#include "../src/fzero_triple_ground.h"

static int parse_positive(const char *text, double *value) {
  char *end;
  *value = strtod(text, &end);
  return end != text && !*end && isfinite(*value) && *value > 0;
}

static void usage(void) {
  fputs("usage: FZeroTripleGroundCapture capture.bin output.ppm "
        "width_mm height_mm eye_mm left_yaw_deg right_yaw_deg bezel_mm "
        "panel_pixels_w panel_pixels_h\n", stderr);
}

static double periodic_error(double a, double b) {
  return remainder(a - b, 1024.0);
}

int main(int argc, char **argv) {
  if (argc != 11) { usage(); return 2; }
  double values[6];
  for (int i = 0; i < 6; ++i) {
    char *end;
    values[i] = strtod(argv[i + 3], &end);
    if (end == argv[i + 3] || *end || !isfinite(values[i])) {
      usage(); return 2;
    }
  }
  double width, height;
  if (!parse_positive(argv[9], &width) || !parse_positive(argv[10], &height) ||
      width > 4096 || height > 2160 || floor(width) != width ||
      floor(height) != height) { usage(); return 2; }
  FzeroTripleRig rig = {values[0], values[1], values[2], 0,
                       values[3], values[4], values[5],
                       (int)width, (int)height};
  const int pw = rig.panel_width_px, ph = rig.panel_height_px;
  FzeroVideoSettings video;
  FzeroVideoStock(&video);
  video.enhanced = true;
  video.aspect = FZERO_ASPECT_FIT;
  const int logical_width = FzeroCalculateViewport(&video, pw, ph).width;
  FzeroTripleSurface panels[3];
  if (!FzeroTripleBuild(&rig, panels)) {
    fputs("invalid physical rig\n", stderr); return 2;
  }
  if (!FzeroRendererLoadCapture(argv[1])) {
    fputs("invalid capture\n", stderr); return 2;
  }
  const FzeroSourceFrame *frame = &frames[current];
  if (frame->ram[0x54] != 2 || frame->ram[0x55] < 3 ||
      (frame->lines[80].registers[offsetof(Ppu, bgmode)] & 7) != 7 ||
      (frame->lines[180].registers[offsetof(Ppu, bgmode)] & 7) != 7) {
    fputs("capture is not an active Mode 7 race frame\n", stderr); return 3;
  }
  memcpy(&scanout, frame->lines[80].registers, PPU_SAVESTATE_REGS_SIZE);
  FzeroMode7Line far_line = FzeroMode7Transform(
      scanout.m7matrix, scanout.m7sel, 81);
  memcpy(&scanout, frame->lines[180].registers, PPU_SAVESTATE_REGS_SIZE);
  FzeroMode7Line near_line = FzeroMode7Transform(
      scanout.m7matrix, scanout.m7sel, 181);
  FzeroTripleGround ground;
  if (!FzeroTripleGroundCalibrate(&rig, logical_width, far_line, 80,
                                  near_line, 180, &ground)) {
    fputs("Mode 7 ground calibration failed\n", stderr); return 3;
  }
  double pitch = atan2(ground.pitch_sin, ground.pitch_cos) *
                 57.295779513082320876;
  if (pitch < 3 || pitch > 35) {
    fprintf(stderr, "camera transition or unsupported pitch: %.2f deg\n", pitch);
    return 3;
  }

  const int span = 3 * pw;
  uint32_t *pixels = calloc((size_t)span * ph, sizeof(*pixels));
  if (!pixels) return 4;
  FzeroCourse course = course_open(frame, true);
  unsigned long long ground_count[3] = {0, 0, 0};
  double center_max_error = 0, center_error_sum = 0;
  unsigned long long center_compared = 0, center_texel_matches = 0;
  FzeroMode7Texel probe[3] = {{0, 0}, {0, 0}, {0, 0}};
  for (int y = 0; y < ph; ++y) {
    int source_y = (int)((y + 0.5) * 224 / ph);
    if (source_y > 223) source_y = 223;
    const FzeroRasterLine *raster = &frame->lines[source_y];
    memcpy(&scanout, raster->registers, PPU_SAVESTATE_REGS_SIZE);
    if ((scanout.bgmode & 7) != 7 || (scanout.inidisp & 128)) continue;
    FzeroMode7Line line = FzeroMode7Transform(
        scanout.m7matrix, scanout.m7sel, (unsigned)source_y + 1);
    FzeroCourseLine reference = course_line(
        course.camera_x, course.camera_y,
        course_centre(scanout.m7matrix, 4),
        course_centre(scanout.m7matrix, 5));
    FzeroTripleVec3 align_left_ray, align_right_ray;
    FzeroMode7Texel align_left, align_right;
    bool can_align = FzeroTripleRay(&panels[1], pw / 2 - 1, y, pw, ph,
                                    &align_left_ray) &&
        FzeroTripleRay(&panels[1], pw / 2, y, pw, ph,
                       &align_right_ray) &&
        FzeroTripleGroundLocate(&ground, align_left_ray, &align_left) &&
        FzeroTripleGroundLocate(&ground, align_right_ray, &align_right);
    for (int panel = 0; panel < 3; ++panel) {
      FzeroCourseCache cache = kCourseCacheEmpty;
      for (int x = 0; x < pw; ++x) {
        FzeroTripleVec3 ray;
        FzeroMode7Texel texel;
        if (!FzeroTripleRay(&panels[panel], x, y, pw, ph, &ray) ||
            !FzeroTripleGroundLocate(&ground, ray, &texel)) continue;
        if (can_align && !FzeroTripleGroundAlignLine(line, align_left,
                align_right, texel, logical_width, pw, &texel)) continue;
        if (panel == 1 && source_y >= 80 && source_y <= 210) {
          double logical_x = 128 + ((x + 0.5) / pw - 0.5) * logical_width;
          FzeroMode7Texel expected = {
              (line.origin_x + logical_x * line.step_x) / 256.0,
              (line.origin_y + logical_x * line.step_y) / 256.0};
          double error = hypot(periodic_error(texel.x, expected.x),
                               periodic_error(texel.y, expected.y));
          if (error > center_max_error) center_max_error = error;
          center_error_sum += error;
          ++center_compared;
          if (((int)floor(texel.x) & 1023) ==
                  ((int)floor(expected.x) & 1023) &&
              ((int)floor(texel.y) & 1023) ==
                  ((int)floor(expected.y) & 1023))
            ++center_texel_matches;
        }
        texel.x = floor(texel.x);
        texel.y = floor(texel.y);
        int tile = course_sample(&course, &reference, &cache, texel);
        unsigned index = FzeroMode7Fetch(&line, frame->vram, texel, tile);
        uint16_t layer = index ? (uint16_t)(0x5000 | index) : 0x500;
        uint16_t main = scanout.screenEnabled[0] & 1 ? layer : 0x500;
        uint16_t sub = scanout.screenEnabled[1] & 1 ? layer : 0x500;
        pixels[(size_t)y * span + panel * pw + x] =
            colour(&scanout, raster->palette, main, sub, false);
        ++ground_count[panel];
        if (x == pw / 2 && y == ph * 3 / 4) probe[panel] = texel;
      }
    }
  }
  if (!center_compared || center_texel_matches * 100 < center_compared * 99 ||
      !ground_count[0] || !ground_count[1] ||
      !ground_count[2] ||
      (fabs(probe[0].x - probe[1].x) < 1 &&
       fabs(probe[0].y - probe[1].y) < 1) ||
      (fabs(probe[2].x - probe[1].x) < 1 &&
       fabs(probe[2].y - probe[1].y) < 1)) {
    fprintf(stderr, "projection validation failed: center_texels=%llu/%llu "
            "ground=%llu,%llu,%llu probes=(%.0f,%.0f),(%.0f,%.0f),(%.0f,%.0f)\n",
            center_texel_matches, center_compared, ground_count[0],
            ground_count[1], ground_count[2], probe[0].x, probe[0].y,
            probe[1].x, probe[1].y, probe[2].x, probe[2].y);
    free(pixels); return 3;
  }
  FILE *output = fopen(argv[2], "wb");
  if (!output) { free(pixels); return 4; }
  fprintf(output, "P6\n%d %d\n255\n", span, ph);
  for (size_t i = 0; i < (size_t)span * ph; ++i) {
    uint32_t color = pixels[i];
    unsigned char rgb[3] = {(unsigned char)(color >> 16),
                            (unsigned char)(color >> 8),
                            (unsigned char)color};
    if (fwrite(rgb, 3, 1, output) != 1) {
      fclose(output); free(pixels); return 4;
    }
  }
  int status = fclose(output) ? 4 : 0;
  fprintf(stderr, "frame=%u pitch=%.2f forward_scale=%.4f "
          "ground_pixels=%llu,%llu,%llu "
          "center_error_texels=%.6f_mean,%.6f_max "
          "center_texel_match=%.2f%% "
          "probes=(%.0f,%.0f),(%.0f,%.0f),(%.0f,%.0f)\n",
          frame->frame, pitch, ground.forward_scale,
          ground_count[0], ground_count[1], ground_count[2],
          center_error_sum / center_compared, center_max_error,
          100.0 * center_texel_matches / center_compared,
          probe[0].x, probe[0].y, probe[1].x, probe[1].y,
          probe[2].x, probe[2].y);
  free(pixels);
  return status;
}
