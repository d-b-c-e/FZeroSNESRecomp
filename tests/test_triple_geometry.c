#include "fzero_triple_geometry.h"
#include "fzero_triple_ground.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static FzeroMode7Line synthetic_line(FzeroTripleRig rig, double y) {
  const double pitch = 10.0 * 0.017453292519943295769;
  const double h = 100.0, camera_x = 500, camera_y = 500;
  const double screen_y = (112 - y) * rig.height_mm / 224;
  const double t = h / (rig.eye_distance_mm * sin(pitch) -
                        screen_y * cos(pitch));
  const double step = t * rig.width_mm / 342;
  const double center = camera_y + t *
      (rig.eye_distance_mm * cos(pitch) + screen_y * sin(pitch));
  return (FzeroMode7Line){(camera_x - 128 * step) * 256,
      center * 256, step * 256, 0, 0};
}

int main(void) {
  FzeroTripleRig rig = {708.4165965748336, 398.4843355733439, 660,
                        0, 70, 70, 8, 2560, 1440};
  FzeroTripleSurface panel[3];
  FzeroTripleVec3 left, right, center;
  CHECK(FzeroTripleBuild(&rig, panel));
  CHECK(FzeroTripleRay(&panel[0], 1280, 720, 2560, 1440, &left));
  CHECK(FzeroTripleRay(&panel[1], 1280, 720, 2560, 1440, &center));
  CHECK(FzeroTripleRay(&panel[2], 1279, 720, 2560, 1440, &right));
  CHECK(left.x < 0 && right.x > 0 && center.z < 0);
  CHECK(fabs(left.x + right.x) < 1e-12);
  CHECK(fabs(left.z - right.z) < 1e-12);
  CHECK(fabs(left.x - center.x) > 0.25); /* three distinct sightlines */
  CHECK(fabs(panel[0].lower_left.x + panel[0].right.x +
             rig.bezel_gap_mm - panel[1].lower_left.x) < 1e-9);
  CHECK(fabs(panel[1].lower_left.x + panel[1].right.x +
             rig.bezel_gap_mm - panel[2].lower_left.x) < 1e-9);
  CHECK(!FzeroTripleRay(&panel[1], 2560, 0, 2560, 1440, &center));
  FzeroTripleGround ground;
  CHECK(FzeroTripleGroundCalibrate(&rig, 342,
      synthetic_line(rig, 80), 80, synthetic_line(rig, 180), 180, &ground));
  CHECK(fabs(ground.camera_height - 100) < 1e-8);
  CHECK(fabs(ground.camera_x - 500) < 1e-8);
  CHECK(fabs(ground.camera_y - 500) < 1e-8);
  /* The center-panel raycaster must reproduce the source Mode 7 scanlines
   * before we can trust different side-panel projections. */
  const double sample_y[] = {80, 180};
  const int sample_x[] = {640, 1280, 1920};
  for (int iy = 0; iy < 2; ++iy) {
    int pixel_y = (int)((sample_y[iy] + 0.5) * 1440 / 224);
    double logical_y = (pixel_y + 0.5) * 224.0 / 1440;
    FzeroMode7Line line = synthetic_line(rig, logical_y);
    for (int ix = 0; ix < 3; ++ix) {
      FzeroTripleVec3 ray;
      FzeroMode7Texel texel;
      CHECK(FzeroTripleRay(&panel[1], sample_x[ix], pixel_y,
                           2560, 1440, &ray));
      CHECK(FzeroTripleGroundLocate(&ground, ray, &texel));
      double logical_x = 128 + ((sample_x[ix] + 0.5) / 2560.0 - 0.5) * 342;
      CHECK(fabs(texel.x - (line.origin_x + logical_x * line.step_x) / 256) < 0.5);
      CHECK(fabs(texel.y - (line.origin_y + logical_x * line.step_y) / 256) < 0.5);
    }
  }
  FzeroMode7Texel left_ground, center_ground, right_ground;
  CHECK(FzeroTripleRay(&panel[0], 1280, 900, 2560, 1440, &left));
  CHECK(FzeroTripleRay(&panel[1], 1280, 900, 2560, 1440, &center));
  CHECK(FzeroTripleRay(&panel[2], 1279, 900, 2560, 1440, &right));
  CHECK(FzeroTripleGroundLocate(&ground, left, &left_ground));
  CHECK(FzeroTripleGroundLocate(&ground, center, &center_ground));
  CHECK(FzeroTripleGroundLocate(&ground, right, &right_ground));
  CHECK(fabs(left_ground.x + right_ground.x - 1000) < 1e-8);
  CHECK(fabs(left_ground.y - right_ground.y) < 1e-8);
  CHECK(fabs(center_ground.x - 500) < 0.1);
  /* Captured active-race frame 1600: Mode 7 scroll has a different forward
   * scale than its horizontal texel step. This guards against assuming an
   * isotropic texture plane merely because synthetic pinhole lines fit. */
  FzeroMode7Line race_far = {100992, 43008, 0, 352, 0};
  FzeroMode7Line race_near = {66944, 73984, 0, 110, 0};
  CHECK(FzeroTripleGroundCalibrate(&rig, 342, race_far, 80,
                                   race_near, 180, &ground));
  CHECK(fabs(ground.forward_scale - 0.4322) < 0.002);
  CHECK(FzeroTripleRay(&panel[1], 1279, 646, 2560, 1440, &center));
  CHECK(FzeroTripleGroundLocate(&ground, center, &center_ground));
  CHECK(fabs(center_ground.x - 335.25) < 1.5);
  CHECK(fabs(center_ground.y - 344.0) < 1.0);
  FzeroMode7Line race_y100 = {85824, 56832, 0, 244, 0};
  FzeroTripleVec3 align_ray_left, align_ray_right;
  FzeroMode7Texel align_left, align_right;
  CHECK(FzeroTripleRay(&panel[1], 1279, 646, 2560, 1440,
                       &align_ray_left));
  CHECK(FzeroTripleRay(&panel[1], 1280, 646, 2560, 1440,
                       &align_ray_right));
  CHECK(FzeroTripleGroundLocate(&ground, align_ray_left, &align_left));
  CHECK(FzeroTripleGroundLocate(&ground, align_ray_right, &align_right));
  const int aligned_samples[] = {640, 1280, 1920};
  for (int i = 0; i < 3; ++i) {
    FzeroMode7Texel raw, corrected;
    int x = aligned_samples[i];
    CHECK(FzeroTripleRay(&panel[1], x, 646, 2560, 1440, &center));
    CHECK(FzeroTripleGroundLocate(&ground, center, &raw));
    CHECK(FzeroTripleGroundAlignLine(race_y100, align_left, align_right,
                                     raw, 342, 2560, &corrected));
    double logical_x = 128 + ((x + 0.5) / 2560.0 - 0.5) * 342;
    CHECK(fabs(corrected.x -
        (race_y100.origin_x + logical_x * race_y100.step_x) / 256) < 1e-6);
    CHECK(fabs(corrected.y -
        (race_y100.origin_y + logical_x * race_y100.step_y) / 256) < 1e-6);
  }
  FzeroMode7Line exact_line = {25600, 51200, 0, 256, 0};
  FzeroMode7Texel exact;
  CHECK(FzeroTripleGroundAlignLine(exact_line,
      (FzeroMode7Texel){0, 0}, (FzeroMode7Texel){0, 0.5},
      (FzeroMode7Texel){0, 0.25 - 1e-12}, 342, 2560, &exact));
  CHECK(exact.x == 100 && exact.y == 328); /* integer-floor stability */
  FzeroMode7Texel race_left, race_right;
  CHECK(FzeroTripleRay(&panel[0], 1280, 900, 2560, 1440, &left));
  CHECK(FzeroTripleRay(&panel[1], 1280, 900, 2560, 1440, &center));
  CHECK(FzeroTripleRay(&panel[2], 1279, 900, 2560, 1440, &right));
  CHECK(FzeroTripleGroundLocate(&ground, left, &race_left));
  CHECK(FzeroTripleGroundLocate(&ground, center, &center_ground));
  CHECK(FzeroTripleGroundLocate(&ground, right, &race_right));
  CHECK(hypot(race_left.x - center_ground.x,
              race_left.y - center_ground.y) > 20);
  CHECK(hypot(race_right.x - center_ground.x,
              race_right.y - center_ground.y) > 20);
  /* Frame 1300 is a transition with a two-line pitch near 79 degrees. */
  FzeroMode7Line transition_far = {298624, 72704, 0, 120, 0};
  FzeroMode7Line transition_near = {286592, 73472, 0, 114, 0};
  CHECK(!FzeroTripleGroundCalibrate(&rig, 342, transition_far, 80,
                                    transition_near, 180, &ground));
  rig.right_yaw_deg = 90;
  CHECK(!FzeroTripleBuild(&rig, panel));
  puts("F-Zero triple-screen physical rays passed");
  return 0;
}
