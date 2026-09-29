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
  for (int side = 0; side < 3; ++side) {
    FzeroTripleVec3 ray;
    double projected_x, projected_y;
    CHECK(FzeroTripleRay(&panel[side], 640, 900, 2560, 1440, &ray));
    CHECK(FzeroTripleProjectDirection(&panel[side], ray, 2560, 1440,
                                      &projected_x, &projected_y));
    CHECK(fabs(projected_x - 640) < 1e-9);
    CHECK(fabs(projected_y - 900) < 1e-9);
  }
  /* A center-facing sprite pixel at a car's depth must retain the source
   * pixel scale on the center panel, while remaining projectable onto the
   * angled panels for the offline side-vehicle preview. */
  FzeroTripleVec3 car_anchor = {20, -80, -500};
  double center_anchor_px_x, center_anchor_px_y, sprite_x, sprite_y;
  CHECK(FzeroTripleProjectDirection(&panel[1], car_anchor, 2560, 1440,
                                    &center_anchor_px_x, &center_anchor_px_y));
  FzeroTripleVec3 sprite_point = {
      car_anchor.x + 10 * rig.width_mm / 342 * (500 / rig.eye_distance_mm),
      car_anchor.y - 8 * rig.height_mm / 224 * (500 / rig.eye_distance_mm),
      car_anchor.z};
  CHECK(FzeroTripleProjectDirection(&panel[1], sprite_point, 2560, 1440,
                                    &sprite_x, &sprite_y));
  CHECK(fabs(sprite_x - center_anchor_px_x - 10.0 * 2560 / 342) < 1e-9);
  CHECK(fabs(sprite_y - center_anchor_px_y - 8.0 * 1440 / 224) < 1e-9);
  for (int side = 0; side < 3; side += 2) {
    FzeroTripleVec3 ray;
    CHECK(FzeroTripleProjectDirection(&panel[side], sprite_point, 2560, 1440,
                                      &sprite_x, &sprite_y));
    int x = (int)lround(sprite_x), y = (int)lround(sprite_y);
    if (x < 0 || x >= 2560 || y < 0 || y >= 1440) continue;
    CHECK(FzeroTripleRay(&panel[side], x, y, 2560, 1440, &ray));
    double depth = sprite_point.z / ray.z;
    CHECK(fabs(depth * ray.x - sprite_point.x) < 2);
    CHECK(fabs(depth * ray.y - sprite_point.y) < 2);
  }
  /* An object's anchor can leave the center while its billboard still
   * intersects both panels. Anchor-only visibility clips that seam art. */
  FzeroTripleRig seam_rig = rig;
  seam_rig.panel_width_px = 512;
  seam_rig.panel_height_px = 288;
  FzeroTripleSurface seam_panels[3];
  FzeroTripleVec3 seam_ray;
  CHECK(FzeroTripleBuild(&seam_rig, seam_panels));
  CHECK(FzeroTripleRay(&seam_panels[2], 6, 148, 512, 288, &seam_ray));
  FzeroTripleVec3 seam_anchor = {
      seam_ray.x * (-500 / seam_ray.z),
      seam_ray.y * (-500 / seam_ray.z), -500};
  double seam_center_x, seam_center_y, seam_left_x, seam_left_y;
  CHECK(FzeroTripleProjectDirection(&seam_panels[1], seam_anchor,
                                    512, 288, &seam_center_x, &seam_center_y));
  FzeroTripleVec3 seam_left = {
      seam_anchor.x - 16 * seam_rig.width_mm / 342 *
                       (500 / seam_rig.eye_distance_mm),
      seam_anchor.y, seam_anchor.z};
  CHECK(FzeroTripleProjectDirection(&seam_panels[1], seam_left,
                                    512, 288, &seam_left_x, &seam_left_y));
  CHECK(seam_center_x >= 512 && seam_left_x < 512);
  CHECK(fabs(seam_center_y - seam_left_y) < 1e-9);
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
  for (int side = 0; side < 3; ++side)
    for (int ix = 0; ix < 3; ++ix) {
      double horizon;
      CHECK(FzeroTripleGroundHorizon(&ground, &panel[side], sample_x[ix],
                                    2560, 1440, &horizon));
      double u = (sample_x[ix] + 0.5) / 2560.0;
      double v = 1.0 - (horizon + 0.5) / 1440.0;
      double py = panel[side].lower_left.y + u * panel[side].right.y +
                  v * panel[side].up.y;
      double pz = panel[side].lower_left.z + u * panel[side].right.z +
                  v * panel[side].up.z;
      CHECK(fabs(-pz * ground.pitch_sin - py * ground.pitch_cos) < 1e-10);
    }
  FzeroTripleRig raised_rig = rig;
  raised_rig.eye_height_mm = 120;
  FzeroTripleSurface raised_panels[3];
  CHECK(FzeroTripleBuild(&raised_rig, raised_panels));
  double raised_horizon;
  CHECK(FzeroTripleGroundHorizon(&ground, &raised_panels[0], 1280,
                                2560, 1440, &raised_horizon));
  FzeroTripleVec3 raised_top, raised_bottom;
  CHECK(FzeroTripleRay(&raised_panels[0], 1280, 0, 2560, 1440, &raised_top));
  CHECK(FzeroTripleRay(&raised_panels[0], 1280, 1439, 2560, 1440,
                       &raised_bottom));
  double raised_d0 = -raised_top.z * ground.pitch_sin -
                     raised_top.y * ground.pitch_cos;
  double raised_d1 = -raised_bottom.z * ground.pitch_sin -
                     raised_bottom.y * ground.pitch_cos;
  /* With an eye above panel center, interpolating normalized end rays can
   * miss the true horizon by over nine pixels on this angled panel. */
  CHECK(fabs(raised_horizon -
             (-raised_d0 * 1439 / (raised_d1 - raised_d0))) > 9);
  double raised_u = (1280.5 / 2560.0);
  double raised_v = 1.0 - (raised_horizon + 0.5) / 1440.0;
  double raised_y = raised_panels[0].lower_left.y +
                    raised_u * raised_panels[0].right.y +
                    raised_v * raised_panels[0].up.y;
  double raised_z = raised_panels[0].lower_left.z +
                    raised_u * raised_panels[0].right.z +
                    raised_v * raised_panels[0].up.z;
  CHECK(fabs(-raised_z * ground.pitch_sin -
             raised_y * ground.pitch_cos) < 1e-10);
  /* A world-ground anchor must invert to the exact physical panel pixel.
   * This is the geometric prerequisite for placing car sprites on the sides;
   * it does not imply their artwork or guest OAM is available there. */
  const int anchor_x[] = {640, 1280, 1920};
  const int anchor_y[] = {900, 1200};
  for (int side = 0; side < 3; ++side)
    for (int ix = 0; ix < 3; ++ix)
      for (int iy = 0; iy < 2; ++iy) {
        FzeroTripleVec3 ray;
        FzeroMode7Texel texel, row_texel;
        FzeroTripleGroundRow row;
        double projected_x, projected_y;
        CHECK(FzeroTripleGroundBuildRow(&ground, &panel[side],
                                       anchor_y[iy], 2560, 1440, &row));
        CHECK(FzeroTripleRay(&panel[side], anchor_x[ix], anchor_y[iy],
                             2560, 1440, &ray));
        CHECK(FzeroTripleGroundLocate(&ground, ray, &texel));
        CHECK(FzeroTripleGroundRowLocate(&row, anchor_x[ix], &row_texel));
        CHECK(fabs(row_texel.x - texel.x) < 1e-8);
        CHECK(fabs(row_texel.y - texel.y) < 1e-8);
        CHECK(FzeroTripleGroundProject(&ground, &panel[side], texel,
                                      2560, 1440, &projected_x, &projected_y));
        CHECK(fabs(projected_x - anchor_x[ix]) < 1e-7);
        CHECK(fabs(projected_y - anchor_y[iy]) < 1e-7);
      }
  CHECK(!FzeroTripleGroundProject(&ground, &panel[1], center_ground,
                                 0, 1440, NULL, NULL));
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
  /* The captured race has anisotropic forward scale, unlike the synthetic
   * fixture above. Its rays must still invert on every angled panel. */
  for (int side = 0; side < 3; ++side)
    for (int ix = 0; ix < 3; ++ix)
      for (int iy = 0; iy < 2; ++iy) {
        FzeroTripleVec3 ray;
        FzeroMode7Texel texel, row_texel;
        FzeroTripleGroundRow row;
        double projected_x, projected_y;
        CHECK(FzeroTripleGroundBuildRow(&ground, &panel[side],
                                       anchor_y[iy], 2560, 1440, &row));
        CHECK(FzeroTripleRay(&panel[side], anchor_x[ix], anchor_y[iy],
                             2560, 1440, &ray));
        CHECK(FzeroTripleGroundLocate(&ground, ray, &texel));
        CHECK(FzeroTripleGroundRowLocate(&row, anchor_x[ix], &row_texel));
        CHECK(fabs(row_texel.x - texel.x) < 1e-8);
        CHECK(fabs(row_texel.y - texel.y) < 1e-8);
        CHECK(FzeroTripleGroundProject(&ground, &panel[side], texel,
                                      2560, 1440, &projected_x, &projected_y));
        CHECK(fabs(projected_x - anchor_x[ix]) < 1e-7);
        CHECK(fabs(projected_y - anchor_y[iy]) < 1e-7);
      }
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
  FzeroTripleLineAlignment alignment;
  FzeroMode7Texel batched;
  CHECK(FzeroTripleGroundBuildLineAlignment(exact_line,
      (FzeroMode7Texel){0, 0}, (FzeroMode7Texel){0, 0.5},
      342, 2560, &alignment));
  CHECK(FzeroTripleGroundApplyLineAlignment(&alignment,
      (FzeroMode7Texel){0, 0.25 - 1e-12}, &batched));
  CHECK(batched.x == exact.x && batched.y == exact.y);
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
  /* Captured race frame 1800: the two visible opponents' WRAM world anchors
   * should invert near the guest's projected $0C50/$0C60 screen anchors.
   * Sprite top-left and ground contact are deliberately not equated. */
  FzeroMode7Line car_far = {126080, 43008, 0, 352, 0};
  FzeroMode7Line car_near = {92032, 73984, 0, 110, 0};
  CHECK(FzeroTripleGroundCalibrate(&rig, 342, car_far, 80,
                                   car_near, 180, &ground));
  double car_x, car_y;
  FzeroMode7Texel car_texel;
  CHECK(FzeroTripleGroundWorldTexel(3503, 384, 3434, 344,
      (FzeroMode7Texel){362, 344}, &car_texel));
  CHECK(FzeroTripleGroundProject(&ground, &panel[1],
      car_texel, 2560, 1440, &car_x, &car_y));
  CHECK(fabs(128 + (car_x + 0.5 - 1280) * 342 / 2560 - 172) < 3);
  CHECK(fabs((car_y + 0.5) * 224 / 1440 - 105) < 5);
  CHECK(FzeroTripleGroundWorldTexel(3729, 397, 3434, 344,
      (FzeroMode7Texel){362, 344}, &car_texel));
  CHECK(FzeroTripleGroundProject(&ground, &panel[1],
      car_texel, 2560, 1440, &car_x, &car_y));
  CHECK(fabs(128 + (car_x + 0.5 - 1280) * 342 / 2560 - 149) < 3);
  CHECK(fabs((car_y + 0.5) * 224 / 1440 - 61) < 5);
  CHECK(FzeroTripleGroundWorldTexel(10, 4088, 8180, 12,
      (FzeroMode7Texel){500, 500}, &car_texel));
  CHECK(car_texel.x == 522 && car_texel.y == 480);
  CHECK(!FzeroTripleGroundWorldTexel(1, 1, 0, 0,
      (FzeroMode7Texel){NAN, 0}, &car_texel));
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
