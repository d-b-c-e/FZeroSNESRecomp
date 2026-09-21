#include "fzero_mods.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%d: %s\n", __LINE__, #e); exit(1); } } while (0)
int main(void) {
  FzeroVideoSettings s, loaded; FzeroVideoStock(&s); /* start from nothing enabled to test each toggle */
  const RecompLauncherCModProvider *p = FzeroModsProvider(&s, "test-mods.ini");
  RecompLauncherCModFeature w, f;
  RecompLauncherCModOption option;
  CHECK(p->package_count(NULL) == 4 && p->feature_count(NULL) == 4);
  RecompLauncherCModFeature hd;
  CHECK(p->feature_get(NULL, 3, &hd) && !hd.enabled && hd.option_count == 1);
  CHECK(p->feature_enable(NULL, hd.package_id, hd.id, 1));
  CHECK(s.hd_mode7 && !s.enhanced && !s.fps_enabled);
  CHECK(p->feature_set_option(NULL, hd.package_id, hd.id, "scale", "4x"));
  CHECK(s.hd_scale == 4);
  CHECK(!p->feature_set_option(NULL, hd.package_id, hd.id, "scale", "8x"));
  CHECK(p->feature_option_get(NULL, hd.package_id, hd.id, 0, &option));
  CHECK(!strcmp(option.value, "4x") && !strcmp(option.default_value, "2x"));
  RecompLauncherCModFeature deluxe;
  CHECK(p->feature_get(NULL, 2, &deluxe) && deluxe.option_count == 0);
  CHECK(p->feature_enable(NULL, deluxe.package_id, deluxe.id, 1));
  CHECK(s.bs_deluxe && !s.fps_enabled && !s.enhanced);
  CHECK(!p->feature_option_get(NULL, deluxe.package_id, deluxe.id, 0, &option));
  CHECK(p->feature_get(NULL, 0, &w) && p->feature_get(NULL, 1, &f));
  CHECK(strcmp(w.package_id, f.package_id) && w.option_count == 1 && f.option_count == 1);
  CHECK(p->feature_enable(NULL, f.package_id, f.id, 1));
  CHECK(s.fps_enabled && !s.enhanced);
  CHECK(p->feature_set_option(NULL, f.package_id, f.id, "fps", "144"));
  CHECK(!p->feature_set_option(NULL, w.package_id, w.id, "fps", "60"));
  CHECK(p->feature_enable(NULL, w.package_id, w.id, 1));
  CHECK(p->feature_enable(NULL, f.package_id, f.id, 0));
  CHECK(s.enhanced && !s.fps_enabled && s.fps == 144);
  CHECK(p->feature_option_get(NULL, w.package_id, w.id, 0, &option) && !strcmp(option.id, "aspect"));
  CHECK(p->feature_option_get(NULL, f.package_id, f.id, 0, &option) && !strcmp(option.id, "fps"));
  CHECK(p->commit(NULL, NULL) && FzeroVideoLoad(&loaded, "test-mods.ini"));
  CHECK(loaded.enhanced && !loaded.fps_enabled && loaded.fps == 144 && loaded.bs_deluxe);
  CHECK(loaded.hd_mode7 && loaded.hd_scale == 4);
  CHECK(p->feature_enable(NULL, hd.package_id, hd.id, 0));
  CHECK(!s.hd_mode7 && s.hd_scale == 4 && s.enhanced);
  CHECK(p->feature_enable(NULL, deluxe.package_id, deluxe.id, 0));
  CHECK(!s.bs_deluxe && s.enhanced && !s.fps_enabled);
  remove("test-mods.ini");
  puts("Independent widescreen and presentation FPS plugins passed");
  return 0;
}
