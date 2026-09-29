#pragma once

#include "fzero_mode7.h"
#include "fzero_triple_geometry.h"

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
