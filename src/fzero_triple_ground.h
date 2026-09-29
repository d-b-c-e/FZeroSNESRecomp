#pragma once

#include "fzero_mode7.h"
#include "fzero_triple_geometry.h"
#include <math.h>

/* Calibrated flat-ground camera in Mode 7 texture coordinates. This is a
 * renderer experiment, not a claim about world-space SNES sprites. */
typedef struct FzeroTripleGround {
  double camera_x, camera_y;
  double right_x, right_y, forward_x, forward_y;
  double camera_height, forward_scale, pitch_sin, pitch_cos;
} FzeroTripleGround;

bool FzeroTripleGroundCalibrate(const FzeroTripleRig *rig, int logical_width,
                               FzeroMode7Line far_line, double far_y,
                               FzeroMode7Line near_line, double near_y,
                               FzeroTripleGround *out);
bool FzeroTripleGroundLocate(const FzeroTripleGround *ground,
                             FzeroTripleVec3 ray, FzeroMode7Texel *texel);
/* For a fixed physical-panel row, the flat-ground intersection is the ratio
 * of affine numerators and an affine horizon denominator in pixel X. */
typedef struct FzeroTripleGroundRow {
  double camera_x, camera_y;
  double down, down_step, x_num, x_step, y_num, y_step;
} FzeroTripleGroundRow;
bool FzeroTripleGroundBuildRow(const FzeroTripleGround *ground,
                               const FzeroTripleSurface *panel,
                               int y, int width, int height,
                               FzeroTripleGroundRow *out);
bool FzeroTripleGroundRowLocate(const FzeroTripleGroundRow *row,
                                int x, FzeroMode7Texel *texel);
/* Same guarded row intersection in the renderer's per-pixel translation unit;
 * the public wrapper remains for diagnostic callers and tests. */
static inline bool FzeroTripleGroundRowLocateInline(
    const FzeroTripleGroundRow *row, int x, FzeroMode7Texel *texel) {
  if (!row || !texel || x < 0) return false;
  double down = row->down + x * row->down_step;
  if (!(down > 1e-9) || !isfinite(down)) return false;
  double reciprocal = 1.0 / down;
  texel->x = row->camera_x + (row->x_num + x * row->x_step) * reciprocal;
  texel->y = row->camera_y + (row->y_num + x * row->y_step) * reciprocal;
  return isfinite(texel->x) && isfinite(texel->y) &&
         fabs(texel->x) < 1e6 && fabs(texel->y) < 1e6;
}
/* Exact fractional pixel row where a column's unnormalized panel ray is
 * parallel to the ground plane; unlike normalized end-ray interpolation,
 * the denominator is affine in screen Y. */
bool FzeroTripleGroundHorizon(const FzeroTripleGround *ground,
                              const FzeroTripleSurface *panel,
                              int x, int width, int height,
                              double *pixel_y);
/* Convert a nearby course-world anchor to the camera's unwrapped Mode 7
 * representative. F-Zero's course repeats every 8192x4096 world units. */
bool FzeroTripleGroundWorldTexel(int world_x, int world_y,
                                 int camera_world_x, int camera_world_y,
                                 FzeroMode7Texel mode7_center,
                                 FzeroMode7Texel *texel);
/* Inverse of Locate for a flat-ground point in the same unwrapped texture
 * coordinate representative as the calibrated camera. Returns subpixel panel
 * coordinates, including points outside a panel for caller-side clipping. */
bool FzeroTripleGroundProject(const FzeroTripleGround *ground,
                              const FzeroTripleSurface *panel,
                              FzeroMode7Texel texel, int panel_width,
                              int panel_height, double *pixel_x,
                              double *pixel_y);
/* Per-row similarity correction: map two center-panel physical samples to
 * the exact captured Mode 7 step and center, then apply that same transform
 * to all three panels. This preserves panel-specific perspective while the
 * retail Q8 scroll/rounding remains pixel-exact on the center panel. */
bool FzeroTripleGroundAlignLine(FzeroMode7Line line,
                                FzeroMode7Texel center_left,
                                FzeroMode7Texel center_right,
                                FzeroMode7Texel raw,
                                int logical_width, int panel_width,
                                FzeroMode7Texel *aligned);
typedef struct FzeroTripleLineAlignment {
  double center_x, center_y, raw_center_x, raw_center_y, a, b;
} FzeroTripleLineAlignment;
bool FzeroTripleGroundBuildLineAlignment(FzeroMode7Line line,
                                         FzeroMode7Texel center_left,
                                         FzeroMode7Texel center_right,
                                         int logical_width, int panel_width,
                                         FzeroTripleLineAlignment *out);
bool FzeroTripleGroundApplyLineAlignment(const FzeroTripleLineAlignment *alignment,
                                         FzeroMode7Texel raw,
                                         FzeroMode7Texel *aligned);
/* Hot side-panel loop uses the same math in this translation unit so the
 * compiler can inline the per-pixel transform without link-time optimization. */
static inline bool FzeroTripleGroundApplyLineAlignmentInline(
    const FzeroTripleLineAlignment *alignment, FzeroMode7Texel raw,
    FzeroMode7Texel *aligned) {
  if (!alignment || !aligned) return false;
  double dx = raw.x - alignment->raw_center_x;
  double dy = raw.y - alignment->raw_center_y;
  aligned->x = alignment->center_x + alignment->a * dx - alignment->b * dy;
  aligned->y = alignment->center_y + alignment->b * dx + alignment->a * dy;
  /* Avoid flooring one ULP below a nominally exact Mode 7 integer. */
  double rounded_x = nearbyint(aligned->x);
  double rounded_y = nearbyint(aligned->y);
  if (fabs(aligned->x - rounded_x) < 1e-7) aligned->x = rounded_x;
  if (fabs(aligned->y - rounded_y) < 1e-7) aligned->y = rounded_y;
  return isfinite(aligned->x) && isfinite(aligned->y) &&
         fabs(aligned->x) < 1e6 && fabs(aligned->y) < 1e6;
}
