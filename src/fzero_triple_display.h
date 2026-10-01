#pragma once

#include "fzero_video.h"

enum { FZERO_TRIPLE_MAX_DISPLAYS = 16 };

typedef struct FzeroTripleDisplaySelection {
  /* Input indices in physical left, center, right order. */
  int index[3];
  FzeroRect bounds[3];
} FzeroTripleDisplaySelection;

/* Select one unambiguous row of three contiguous, equal-sized landscape
 * displays. Reject mirrored/overlapping or vertically offset layouts. */
bool FzeroTripleSelectDisplays(const FzeroRect *displays, int count,
                               FzeroTripleDisplaySelection *selection);
