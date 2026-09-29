#include "fzero_triple_ground.h"

#include <math.h>

static double magnitude(FzeroMode7Line line) {
  return hypot(line.step_x, line.step_y) / 256.0;
}
static double center_x(FzeroMode7Line line) {
  return (line.origin_x + 128 * line.step_x) / 256.0;
}
static double center_y(FzeroMode7Line line) {
  return (line.origin_y + 128 * line.step_y) / 256.0;
}

bool FzeroTripleGroundCalibrate(const FzeroTripleRig *rig, int logical_width,
                               FzeroMode7Line far_line, double far_y,
                               FzeroMode7Line near_line, double near_y,
                               FzeroTripleGround *out) {
  if (!rig || !out || logical_width < 256 || !(far_y >= 0 &&
      near_y < 224 && near_y > far_y) || !(rig->width_mm > 0 &&
      rig->height_mm > 0 && rig->eye_distance_mm > 0)) return false;
  double far_scale = magnitude(far_line), near_scale = magnitude(near_line);
  if (!(far_scale > near_scale && near_scale > 0) ||
      !isfinite(far_scale) || !isfinite(near_scale)) return false;
  /* A flat ground plane viewed through a pinhole has inverse Mode 7 scale
   * affine in scanline Y. Derive its horizon/pitch and camera height from two
   * observed scanlines, in the same units as the captured texture plane. */
  double slope = (1.0 / near_scale - 1.0 / far_scale) / (near_y - far_y);
  double intercept = 1.0 / far_scale + slope * (112.0 - far_y);
  double tangent = rig->height_mm * intercept /
                   (224.0 * rig->eye_distance_mm * slope);
  /* The game briefly reports near-flat inverse scale during camera
   * transitions. That can fit two lines mathematically while implying an
   * 80-degree camera pitch, so reject outside the measured race envelope. */
  if (!(slope > 0 && tangent > 0.0524078 && tangent < 0.7002076 &&
        isfinite(tangent))) return false; /* 3 to 35 degrees */
  double pitch_cos = 1.0 / sqrt(1.0 + tangent * tangent);
  double pitch_sin = tangent * pitch_cos;
  double camera_height = logical_width * rig->height_mm * pitch_cos /
      (rig->width_mm * 224.0 * slope);
  if (!(camera_height > 0 && isfinite(camera_height))) return false;

  double right_x = near_line.step_x / (256.0 * near_scale);
  double right_y = near_line.step_y / (256.0 * near_scale);
  if (!isfinite(right_x) || !isfinite(right_y)) return false;
  double forward_x = -right_y, forward_y = right_x;
  double far_screen_y = (112.0 - far_y) * rig->height_mm / 224.0;
  double far_down = rig->eye_distance_mm * pitch_sin - far_screen_y * pitch_cos;
  if (!(far_down > 0)) return false;
  double far_distance = camera_height *
      (rig->eye_distance_mm * pitch_cos + far_screen_y * pitch_sin) / far_down;
  double near_screen_y = (112.0 - near_y) * rig->height_mm / 224.0;
  double near_down = rig->eye_distance_mm * pitch_sin - near_screen_y * pitch_cos;
  if (!(near_down > 0)) return false;
  double near_distance = camera_height *
      (rig->eye_distance_mm * pitch_cos + near_screen_y * pitch_sin) / near_down;
  double expected = far_distance - near_distance;
  double raw_x = center_x(far_line) - center_x(near_line);
  double raw_y = center_y(far_line) - center_y(near_line);
  double projected = raw_x * forward_x + raw_y * forward_y;
  double transverse = raw_x * right_x + raw_y * right_y;
  if (projected < 0) {
    forward_x = -forward_x; forward_y = -forward_y;
    projected = -projected;
  }
  /* The game's longitudinal Mode 7 scroll is not necessarily in the same
   * texture units as its horizontal scale. Capture that anisotropy explicitly
   * from the observed scanline centers. */
  double forward_scale = projected / expected;
  if (!(forward_scale > 0.05 && forward_scale < 10.0) ||
      fabs(transverse) > 4.0 || !isfinite(forward_scale)) return false;
  *out = (FzeroTripleGround){
      center_x(near_line) - forward_x * near_distance * forward_scale,
      center_y(near_line) - forward_y * near_distance * forward_scale,
      right_x, right_y, forward_x, forward_y,
      camera_height, forward_scale, pitch_sin, pitch_cos};
  return true;
}

bool FzeroTripleGroundLocate(const FzeroTripleGround *ground,
                             FzeroTripleVec3 ray, FzeroMode7Texel *texel) {
  if (!ground || !texel) return false;
  double down = -ray.z * ground->pitch_sin - ray.y * ground->pitch_cos;
  if (!(down > 1e-9) || !isfinite(down)) return false; /* sky */
  double distance = ground->camera_height / down;
  double forward = -ray.z * ground->pitch_cos + ray.y * ground->pitch_sin;
  texel->x = ground->camera_x + distance *
      (ground->right_x * ray.x + ground->forward_x * forward *
       ground->forward_scale);
  texel->y = ground->camera_y + distance *
      (ground->right_y * ray.x + ground->forward_y * forward *
       ground->forward_scale);
  return isfinite(texel->x) && isfinite(texel->y) &&
         fabs(texel->x) < 1e6 && fabs(texel->y) < 1e6;
}

bool FzeroTripleGroundBuildRow(const FzeroTripleGround *ground,
                               const FzeroTripleSurface *panel,
                               int y, int width, int height,
                               FzeroTripleGroundRow *out) {
  if (!ground || !panel || !out || width < 1 || height < 1 ||
      y < 0 || y >= height) return false;
  double u = 0.5 / width;
  double v = 1.0 - (y + 0.5) / height;
  double px = panel->lower_left.x + u * panel->right.x + v * panel->up.x;
  double py = panel->lower_left.y + u * panel->right.y + v * panel->up.y;
  double pz = panel->lower_left.z + u * panel->right.z + v * panel->up.z;
  double sx = panel->right.x / width;
  double sy = panel->right.y / width;
  double sz = panel->right.z / width;
  double forward = -pz * ground->pitch_cos + py * ground->pitch_sin;
  double forward_step = -sz * ground->pitch_cos + sy * ground->pitch_sin;
  *out = (FzeroTripleGroundRow){
      ground->camera_x, ground->camera_y,
      -pz * ground->pitch_sin - py * ground->pitch_cos,
      -sz * ground->pitch_sin - sy * ground->pitch_cos,
      ground->camera_height * (ground->right_x * px +
          ground->forward_x * forward * ground->forward_scale),
      ground->camera_height * (ground->right_x * sx +
          ground->forward_x * forward_step * ground->forward_scale),
      ground->camera_height * (ground->right_y * px +
          ground->forward_y * forward * ground->forward_scale),
      ground->camera_height * (ground->right_y * sx +
          ground->forward_y * forward_step * ground->forward_scale)};
  return isfinite(out->down) && isfinite(out->down_step) &&
         isfinite(out->x_num) && isfinite(out->x_step) &&
         isfinite(out->y_num) && isfinite(out->y_step);
}

bool FzeroTripleGroundRowLocate(const FzeroTripleGroundRow *row,
                                int x, FzeroMode7Texel *texel) {
  if (!row || !texel || x < 0) return false;
  double down = row->down + x * row->down_step;
  if (!(down > 1e-9) || !isfinite(down)) return false;
  double reciprocal = 1.0 / down;
  texel->x = row->camera_x + (row->x_num + x * row->x_step) * reciprocal;
  texel->y = row->camera_y + (row->y_num + x * row->y_step) * reciprocal;
  return isfinite(texel->x) && isfinite(texel->y) &&
         fabs(texel->x) < 1e6 && fabs(texel->y) < 1e6;
}

bool FzeroTripleGroundHorizon(const FzeroTripleGround *ground,
                              const FzeroTripleSurface *panel,
                              int x, int width, int height,
                              double *pixel_y) {
  if (!ground || !panel || !pixel_y || width < 1 || height < 1 ||
      x < 0 || x >= width) return false;
  double u = (x + 0.5) / width;
  double v = 1.0 - 0.5 / height;
  double py = panel->lower_left.y + u * panel->right.y + v * panel->up.y;
  double pz = panel->lower_left.z + u * panel->right.z + v * panel->up.z;
  double top = -pz * ground->pitch_sin - py * ground->pitch_cos;
  double slope = (panel->up.z * ground->pitch_sin +
                  panel->up.y * ground->pitch_cos) / height;
  if (!(fabs(slope) > 1e-12) || !isfinite(slope)) return false;
  *pixel_y = -top / slope;
  return isfinite(*pixel_y);
}

bool FzeroTripleGroundWorldTexel(int world_x, int world_y,
                                 int camera_world_x, int camera_world_y,
                                 FzeroMode7Texel mode7_center,
                                 FzeroMode7Texel *texel) {
  if (!texel || !isfinite(mode7_center.x) || !isfinite(mode7_center.y))
    return false;
  texel->x = mode7_center.x + remainder((double)world_x - camera_world_x,
                                        8192.0);
  texel->y = mode7_center.y + remainder((double)world_y - camera_world_y,
                                        4096.0);
  return isfinite(texel->x) && isfinite(texel->y);
}

static double dot3(FzeroTripleVec3 a, FzeroTripleVec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

bool FzeroTripleGroundProject(const FzeroTripleGround *ground,
                              const FzeroTripleSurface *panel,
                              FzeroMode7Texel texel, int panel_width,
                              int panel_height, double *pixel_x,
                              double *pixel_y) {
  if (!ground || !panel || !pixel_x || !pixel_y || panel_width < 1 ||
      panel_height < 1 || !(ground->forward_scale > 0)) return false;
  double dx = texel.x - ground->camera_x;
  double dy = texel.y - ground->camera_y;
  double lateral = dx * ground->right_x + dy * ground->right_y;
  double forward = (dx * ground->forward_x + dy * ground->forward_y) /
                   ground->forward_scale;
  FzeroTripleVec3 direction = {
      lateral,
      -ground->camera_height * ground->pitch_cos + forward * ground->pitch_sin,
      -ground->camera_height * ground->pitch_sin - forward * ground->pitch_cos};
  FzeroTripleVec3 normal = {
      panel->right.y * panel->up.z - panel->right.z * panel->up.y,
      panel->right.z * panel->up.x - panel->right.x * panel->up.z,
      panel->right.x * panel->up.y - panel->right.y * panel->up.x};
  double denominator = dot3(normal, direction);
  double right_length = dot3(panel->right, panel->right);
  double up_length = dot3(panel->up, panel->up);
  if (!(fabs(denominator) > 1e-9 && right_length > 0 && up_length > 0))
    return false;
  double distance = dot3(normal, panel->lower_left) / denominator;
  if (!(distance > 0) || !isfinite(distance)) return false;
  FzeroTripleVec3 relative = {
      distance * direction.x - panel->lower_left.x,
      distance * direction.y - panel->lower_left.y,
      distance * direction.z - panel->lower_left.z};
  double u = dot3(relative, panel->right) / right_length;
  double v = dot3(relative, panel->up) / up_length;
  *pixel_x = u * panel_width - 0.5;
  *pixel_y = (1.0 - v) * panel_height - 0.5;
  return isfinite(*pixel_x) && isfinite(*pixel_y);
}

bool FzeroTripleGroundAlignLine(FzeroMode7Line line,
                                FzeroMode7Texel center_left,
                                FzeroMode7Texel center_right,
                                FzeroMode7Texel raw,
                                int logical_width, int panel_width,
                                FzeroMode7Texel *aligned) {
  FzeroTripleLineAlignment alignment;
  return FzeroTripleGroundBuildLineAlignment(line, center_left, center_right,
             logical_width, panel_width, &alignment) &&
         FzeroTripleGroundApplyLineAlignment(&alignment, raw, aligned);
}

bool FzeroTripleGroundBuildLineAlignment(FzeroMode7Line line,
                                         FzeroMode7Texel center_left,
                                         FzeroMode7Texel center_right,
                                         int logical_width, int panel_width,
                                         FzeroTripleLineAlignment *out) {
  if (!out || logical_width < 256 || panel_width < 2) return false;
  double px = center_right.x - center_left.x;
  double py = center_right.y - center_left.y;
  double norm = px * px + py * py;
  if (!(norm > 1e-12) || !isfinite(norm)) return false;
  double factor = (double)logical_width / panel_width / 256.0;
  double ox = line.step_x * factor, oy = line.step_y * factor;
  double a = (px * ox + py * oy) / norm;
  double b = (px * oy - py * ox) / norm;
  *out = (FzeroTripleLineAlignment){center_x(line), center_y(line),
      (center_left.x + center_right.x) * 0.5,
      (center_left.y + center_right.y) * 0.5, a, b};
  return true;
}

bool FzeroTripleGroundApplyLineAlignment(const FzeroTripleLineAlignment *alignment,
                                         FzeroMode7Texel raw,
                                         FzeroMode7Texel *aligned) {
  if (!alignment || !aligned) return false;
  double dx = raw.x - alignment->raw_center_x;
  double dy = raw.y - alignment->raw_center_y;
  aligned->x = alignment->center_x + alignment->a * dx - alignment->b * dy;
  aligned->y = alignment->center_y + alignment->b * dx + alignment->a * dy;
  /* A nominally exact integer can land one ULP below it after the inverse
   * rotation. Mode 7 floors texels, so that would shift an entire straight
   * scanline by one pixel despite the analytic center match. */
  double rounded_x = nearbyint(aligned->x);
  double rounded_y = nearbyint(aligned->y);
  if (fabs(aligned->x - rounded_x) < 1e-7) aligned->x = rounded_x;
  if (fabs(aligned->y - rounded_y) < 1e-7) aligned->y = rounded_y;
  return isfinite(aligned->x) && isfinite(aligned->y) &&
         fabs(aligned->x) < 1e6 && fabs(aligned->y) < 1e6;
}
