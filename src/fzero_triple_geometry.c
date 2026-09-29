#include "fzero_triple_geometry.h"

#include <math.h>

static FzeroTripleVec3 add(FzeroTripleVec3 a, FzeroTripleVec3 b) {
  return (FzeroTripleVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}
static FzeroTripleVec3 scale(FzeroTripleVec3 a, double s) {
  return (FzeroTripleVec3){a.x * s, a.y * s, a.z * s};
}

bool FzeroTripleBuild(const FzeroTripleRig *rig, FzeroTripleSurface panels[3]) {
  if (!rig || !panels || !(rig->width_mm > 0 && rig->height_mm > 0 &&
      rig->eye_distance_mm > 0 && rig->bezel_gap_mm >= 0 &&
      rig->left_yaw_deg >= 0 && rig->left_yaw_deg < 90 &&
      rig->right_yaw_deg >= 0 && rig->right_yaw_deg < 90 &&
      rig->panel_width_px > 0 && rig->panel_height_px > 0)) return false;
  const double half = rig->width_mm / 2;
  const double radians = 0.017453292519943295769;
  const FzeroTripleVec3 center = {0, -rig->eye_height_mm,
                                  -rig->eye_distance_mm};
  const FzeroTripleVec3 up = {0, rig->height_mm, 0};
  const FzeroTripleVec3 center_span = {rig->width_mm, 0, 0};
  panels[1] = (FzeroTripleSurface){add(center, (FzeroTripleVec3){-half,
      -rig->height_mm / 2, 0}), center_span, up};

  double left_angle = rig->left_yaw_deg * radians;
  FzeroTripleVec3 left_span = {rig->width_mm * cos(left_angle), 0,
                                -rig->width_mm * sin(left_angle)};
  FzeroTripleVec3 left_hinge = add(center,
      (FzeroTripleVec3){-half - rig->bezel_gap_mm, 0, 0});
  panels[0] = (FzeroTripleSurface){add(left_hinge,
      add(scale(left_span, -1), scale(up, -0.5))), left_span, up};

  double right_angle = rig->right_yaw_deg * radians;
  FzeroTripleVec3 right_span = {rig->width_mm * cos(right_angle), 0,
                                 rig->width_mm * sin(right_angle)};
  FzeroTripleVec3 right_hinge = add(center,
      (FzeroTripleVec3){half + rig->bezel_gap_mm, 0, 0});
  panels[2] = (FzeroTripleSurface){add(right_hinge, scale(up, -0.5)),
                                    right_span, up};
  return true;
}

bool FzeroTripleRay(const FzeroTripleSurface *panel, int x, int y,
                    int width, int height, FzeroTripleVec3 *ray) {
  if (!panel || !ray || width <= 0 || height <= 0 || x < 0 || x >= width ||
      y < 0 || y >= height) return false;
  double u = (x + 0.5) / width;
  double v = 1.0 - (y + 0.5) / height;
  FzeroTripleVec3 point = add(panel->lower_left,
      add(scale(panel->right, u), scale(panel->up, v)));
  double length = sqrt(point.x * point.x + point.y * point.y +
                       point.z * point.z);
  if (!(length > 0) || !isfinite(length)) return false;
  *ray = scale(point, 1.0 / length);
  return true;
}
