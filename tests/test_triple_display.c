#include "fzero_triple_display.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%d: %s\n", __LINE__, #e); exit(1); } } while (0)

int main(void) {
  FzeroTripleDisplaySelection selected;
  FzeroRect shuffled[] = {
    {2560, 0, 2560, 1440},
    {-2560, 0, 2560, 1440},
    {0, 0, 2560, 1440}
  };
  CHECK(FzeroTripleSelectDisplays(shuffled, 3, &selected));
  CHECK(selected.index[0] == 1 && selected.index[1] == 2 &&
        selected.index[2] == 0);
  CHECK(selected.bounds[0].x == -2560 && selected.bounds[1].x == 0 &&
        selected.bounds[2].x == 2560);

  FzeroRect extra[] = {
    {0, 0, 2560, 1440}, {-2560, 0, 2560, 1440},
    {2560, 0, 2560, 1440}, {7680, 0, 3840, 2160}
  };
  CHECK(FzeroTripleSelectDisplays(extra, 4, &selected));
  CHECK(selected.index[1] == 0);
  extra[3] = (FzeroRect){5120, 0, 2560, 1440};
  CHECK(!FzeroTripleSelectDisplays(extra, 4, &selected)); /* ambiguous row */
  shuffled[0].y = 20;
  CHECK(!FzeroTripleSelectDisplays(shuffled, 3, &selected));
  shuffled[0].y = 0;
  shuffled[0].w = 1920;
  CHECK(!FzeroTripleSelectDisplays(shuffled, 3, &selected));
  FzeroRect surround = {0, 0, 7680, 1440};
  CHECK(!FzeroTripleSelectDisplays(&surround, 1, &selected));
  puts("F-Zero separate-display selection passed");
  return 0;
}
