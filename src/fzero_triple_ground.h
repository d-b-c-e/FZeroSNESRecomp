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
