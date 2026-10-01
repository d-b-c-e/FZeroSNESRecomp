#include "fzero_triple_display.h"

#include <stdint.h>
#include <string.h>

bool FzeroTripleSelectDisplays(const FzeroRect *displays, int count,
                               FzeroTripleDisplaySelection *selection) {
  if (!displays || !selection || count < 3 ||
      count > FZERO_TRIPLE_MAX_DISPLAYS) return false;
  FzeroTripleDisplaySelection found = {0};
  int matches = 0;
  for (int left = 0; left < count; ++left) {
    const FzeroRect *a = &displays[left];
    if (!FzeroTriplePanelSupported(a->w, a->h)) continue;
    for (int center = 0; center < count; ++center) {
      const FzeroRect *b = &displays[center];
      if (center == left || a->y != b->y || a->w != b->w || a->h != b->h ||
          (int64_t)a->x + a->w != b->x) continue;
      for (int right = 0; right < count; ++right) {
        const FzeroRect *c = &displays[right];
        if (right == left || right == center || b->y != c->y ||
            b->w != c->w || b->h != c->h ||
            (int64_t)b->x + b->w != c->x) continue;
        if (++matches > 1) return false;
        found.index[0] = left;
        found.index[1] = center;
        found.index[2] = right;
        found.bounds[0] = *a;
        found.bounds[1] = *b;
        found.bounds[2] = *c;
      }
    }
  }
  if (matches != 1) return false;
  *selection = found;
  return true;
}
