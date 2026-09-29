#pragma once

#include <stdbool.h>

/* Native adapter for the DBCE v1 physical rig. Coordinates match the toolkit:
 * eye at (0,0,0), center panel in negative Z, +X right and +Y up. Curved
 * panels are modeled as their visible planar chords. */
typedef struct FzeroTripleVec3 { double x, y, z; } FzeroTripleVec3;
typedef struct FzeroTripleSurface {
  FzeroTripleVec3 lower_left, right, up;
} FzeroTripleSurface;
typedef struct FzeroTripleRig {
  double width_mm, height_mm, eye_distance_mm, eye_height_mm;
  double left_yaw_deg, right_yaw_deg, bezel_gap_mm;
  int panel_width_px, panel_height_px;
} FzeroTripleRig;

bool FzeroTripleBuild(const FzeroTripleRig *rig, FzeroTripleSurface panels[3]);
bool FzeroTripleRay(const FzeroTripleSurface *panel, int x, int y,
                    int width, int height, FzeroTripleVec3 *ray);
/* Project a camera-space direction onto one physical panel. Coordinates may
 * lie beyond panel bounds; the caller clips them if needed. */
bool FzeroTripleProjectDirection(const FzeroTripleSurface *panel,
                                  FzeroTripleVec3 direction,
                                  int width, int height,
                                  double *pixel_x, double *pixel_y);
