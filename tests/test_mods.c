#include "fzero_mods.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%d: %s\n", __LINE__, #e); exit(1); } } while (0)
static int wrote_wheel_range, wrote_rewind, wrote_ffb, wrote_impact, wrote_brake_invert;
static char wrote_device[256], wrote_impact_type[32];
static int live_axis_value;
static int read_live_axis(const char *guid, int axis, int *value) {
  CHECK(!strcmp(guid, "test-guid"));
  if (axis != 0) return 0;
  *value = live_axis_value;
  return 1;
}
static int list_ffb(char names[][256], int max_devices) {
  CHECK(max_devices >= 2);
  strcpy(names[0], "MOZA R12 Base");
  strcpy(names[1], "Other FFB wheel");
  return 2;
}
static void note_ini(const char *path, const char *section,
                     const char *key, const char *value) {
  CHECK(!strcmp(path, "wheel-options.ini"));
  if (!strcmp(section, "Controller.test-guid") &&
      !strcmp(key, "SteeringRangePercent")) wrote_wheel_range = atoi(value);
  if (!strcmp(section, "Controller.test-guid") &&
      !strcmp(key, "ButtonRewind")) wrote_rewind = atoi(value);
  if (!strcmp(section, "ForceFeedback") &&
      !strcmp(key, "Strength")) wrote_ffb = atoi(value);
  if (!strcmp(section, "ForceFeedback") &&
      !strcmp(key, "ImpactStrength")) wrote_impact = atoi(value);
  if (!strcmp(section, "ForceFeedback") &&
      !strcmp(key, "ImpactType")) strcpy(wrote_impact_type, value);
  if (!strcmp(section, "ForceFeedback") &&
      !strcmp(key, "Device")) strcpy(wrote_device, value);
  if (!strcmp(section, "Controller.test-guid") &&
      !strcmp(key, "BrakeInvert")) wrote_brake_invert = atoi(value);
}
int main(void) {
  FzeroVideoSettings s, loaded; FzeroVideoStock(&s); /* start from nothing enabled to test each toggle */
  const RecompLauncherCModProvider *p = FzeroModsProvider(&s, "test-mods.ini");
  RecompLauncherCModFeature w, f;
  RecompLauncherCModOption option;
  CHECK(p->package_count(NULL) == 7 && p->feature_count(NULL) == 7);
  RecompLauncherCModFeature triple;
  CHECK(p->feature_get(NULL, 5, &triple) && !triple.enabled && triple.option_count == 7);
  CHECK(p->feature_enable(NULL, triple.package_id, triple.id, 1) && s.triple_screen);
  CHECK(p->feature_option_get(NULL, triple.package_id, triple.id, 0, &option));
  CHECK(!strcmp(option.id, "TriplePanelWidthMm") && !strcmp(option.value, "708") &&
        option.min_value == 200 && option.max_value == 3000);
  CHECK(p->feature_option_get(NULL, triple.package_id, triple.id, 6, &option));
  CHECK(!strcmp(option.id, "TripleOutputMode") && !strcmp(option.value, "Span") &&
        option.type == RECOMP_MOD_OPTION_CHOICE && option.choice_count == 2);
  RecompLauncherCModChoice triple_choice;
  CHECK(p->feature_choice_get(NULL, triple.package_id, triple.id,
                              "TripleOutputMode", 1, &triple_choice));
  CHECK(!strcmp(triple_choice.value, "Separate"));
  CHECK(p->feature_set_option(NULL, triple.package_id, triple.id,
                              "TripleOutputMode", "Separate"));
  CHECK(!p->feature_set_option(NULL, triple.package_id, triple.id,
                               "TripleOutputMode", "invalid"));
  CHECK(p->feature_set_option(NULL, triple.package_id, triple.id, "TriplePanelWidthMm", "620"));
  CHECK(p->feature_set_option(NULL, triple.package_id, triple.id, "TripleEyeDistanceMm", "780"));
  CHECK(p->feature_set_option(NULL, triple.package_id, triple.id, "TripleLeftAngleDeg", "55"));
  CHECK(p->feature_set_option(NULL, triple.package_id, triple.id, "TripleRightAngleDeg", "65"));
  CHECK(p->feature_set_option(NULL, triple.package_id, triple.id, "TripleBezelGapMm", "12"));
  CHECK(p->feature_set_option(NULL, triple.package_id, triple.id, "TripleEyeHeightMm", "45"));
  CHECK(!p->feature_set_option(NULL, triple.package_id, triple.id, "TriplePanelWidthMm", "0"));
  CHECK(!p->feature_set_option(NULL, triple.package_id, triple.id, "TripleRightAngleDeg", "90"));
  CHECK(!p->feature_set_option(NULL, triple.package_id, triple.id, "TripleEyeDistanceMm", "660junk"));
  RecompLauncherCModFeature flash;
  CHECK(p->feature_get(NULL, 6, &flash) && !flash.enabled && flash.option_count == 0);
  CHECK(p->feature_enable(NULL, flash.package_id, flash.id, 1) && s.reduce_crash_flash);
  RecompLauncherCModFeature diag;
  CHECK(p->feature_get(NULL, 4, &diag) && !diag.enabled && diag.option_count == 0);
  CHECK(!p->feature_option_get(NULL, diag.package_id, diag.id, 0, &option));
  CHECK(p->feature_enable(NULL, diag.package_id, diag.id, 1) && s.diagnostics);
  CHECK(!s.enhanced && !s.hd_mode7 && !s.fps_enabled);
  RecompLauncherCModFeature hd;
  CHECK(p->feature_get(NULL, 3, &hd) && !hd.enabled && hd.option_count == 1);
  CHECK(p->feature_enable(NULL, hd.package_id, hd.id, 1));
  CHECK(s.hd_mode7 && !s.enhanced && !s.fps_enabled);
  CHECK(p->feature_set_option(NULL, hd.package_id, hd.id, "scale", "4"));
  CHECK(s.hd_scale == 4);
  CHECK(p->feature_get(NULL, 3, &hd) && !strstr(hd.description, "Warning:"));
  for (unsigned scale = 5; scale <= 10; ++scale) {
    char text[8]; snprintf(text, sizeof(text), "%u", scale);
    CHECK(p->feature_set_option(NULL, hd.package_id, hd.id, "scale", text));
    CHECK(s.hd_scale == scale);
    CHECK(p->feature_get(NULL, 3, &hd));
    CHECK(strstr(hd.description, "Warning:") && strstr(hd.description, "own risk"));
    CHECK(!hd.has_error); /* high-cost values remain usable */
  }
  CHECK(!p->feature_set_option(NULL, hd.package_id, hd.id, "scale", "11"));
  CHECK(!p->feature_set_option(NULL, hd.package_id, hd.id, "scale", "6.5"));
  CHECK(s.hd_scale == 10 && strstr(p->last_error(NULL), "2 to 10"));
  CHECK(p->feature_option_get(NULL, hd.package_id, hd.id, 0, &option));
  CHECK(!strcmp(option.value, "10") && !strcmp(option.default_value, "2"));
  CHECK(option.type == RECOMP_MOD_OPTION_INTEGER && option.min_value == 2 && option.max_value == 10);
  RecompLauncherCModFeature deluxe;
  CHECK(p->feature_get(NULL, 2, &deluxe) && deluxe.option_count == 0);
#ifdef FZERO_HAS_DELUXE
  CHECK(p->feature_enable(NULL, deluxe.package_id, deluxe.id, 1));
  CHECK(s.bs_deluxe && !s.fps_enabled && !s.enhanced);
#else
  CHECK(!deluxe.enabled && strstr(deluxe.status, "Unavailable"));
  CHECK(!p->feature_enable(NULL, deluxe.package_id, deluxe.id, 1));
  CHECK(!s.bs_deluxe && strstr(p->last_error(NULL), "unavailable"));
#endif
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
  CHECK(loaded.enhanced && !loaded.fps_enabled && loaded.fps == 144);
#ifdef FZERO_HAS_DELUXE
  CHECK(loaded.bs_deluxe);
#else
  CHECK(!loaded.bs_deluxe);
#endif
  CHECK(loaded.hd_mode7 && loaded.hd_scale == 10 && loaded.diagnostics &&
        loaded.triple_screen && loaded.reduce_crash_flash);
  CHECK(loaded.triple_panel_width_mm == 620 && loaded.triple_eye_distance_mm == 780 &&
        loaded.triple_left_yaw_deg == 55 && loaded.triple_right_yaw_deg == 65 &&
        loaded.triple_bezel_gap_mm == 12 && loaded.triple_eye_height_mm == 45 &&
        loaded.triple_output_mode == FZERO_TRIPLE_OUTPUT_SEPARATE);
  CHECK(p->feature_enable(NULL, diag.package_id, diag.id, 0) && !s.diagnostics);
  CHECK(p->commit(NULL, NULL) && FzeroVideoLoad(&loaded, "test-mods.ini") && !loaded.diagnostics);
  CHECK(p->feature_enable(NULL, hd.package_id, hd.id, 0));
  CHECK(!s.hd_mode7 && s.hd_scale == 10 && s.enhanced);
  CHECK(p->feature_enable(NULL, deluxe.package_id, deluxe.id, 0));
  CHECK(!s.bs_deluxe && s.enhanced && !s.fps_enabled);
  p = FzeroModsProviderWheel(&s, "test-mods.ini", "wheel-options.ini",
                             "test-guid", note_ini, list_ffb, read_live_axis);
  CHECK(p->feature_count(NULL) == 9);
  RecompLauncherCModFeature wheel, ffb, triple_with_wheel;
  CHECK(p->feature_get(NULL, 5, &wheel) && wheel.option_count == 23);
  live_axis_value = 8192;
  CHECK(p->feature_get(NULL, 5, &wheel));
  CHECK(strstr(wheel.status, "RIGHT 50% of SNES frames") &&
        strstr(wheel.status, "axis 0 +25%"));
  CHECK(p->feature_set_option(NULL, wheel.package_id, wheel.id,
                              "SteeringRangePercent", "50"));
  CHECK(p->feature_get(NULL, 5, &wheel) &&
        strstr(wheel.status, "RIGHT 71% of SNES frames"));
  live_axis_value = -8192;
  CHECK(p->feature_get(NULL, 5, &wheel) &&
        strstr(wheel.status, "LEFT 71% of SNES frames"));
  live_axis_value = 0;
  CHECK(p->feature_get(NULL, 5, &wheel) &&
        strstr(wheel.status, "CENTER 0% of SNES frames"));
  CHECK(p->feature_get(NULL, 6, &ffb) && ffb.option_count == 4);
  CHECK(p->feature_get(NULL, 7, &triple_with_wheel) &&
        !strcmp(triple_with_wheel.package_id, triple.package_id) &&
        triple_with_wheel.enabled && triple_with_wheel.option_count == 7);
  RecompLauncherCModFeature flash_with_wheel;
  CHECK(p->feature_get(NULL, 8, &flash_with_wheel) &&
        !strcmp(flash_with_wheel.package_id, flash.package_id) &&
        flash_with_wheel.enabled && flash_with_wheel.option_count == 0);
  CHECK(p->feature_option_get(NULL, wheel.package_id, wheel.id, 22, &option));
  CHECK(option.type == RECOMP_MOD_OPTION_RAW_BUTTON);
  CHECK(!strcmp(option.device_guid, "test-guid"));
  CHECK(p->feature_option_get(NULL, wheel.package_id, wheel.id, 3, &option));
  CHECK(option.type == RECOMP_MOD_OPTION_RAW_AXIS);
  CHECK(!strcmp(option.device_guid, "test-guid"));
  CHECK(p->feature_option_get(NULL, wheel.package_id, wheel.id, 8, &option));
  CHECK(option.type == RECOMP_MOD_OPTION_BOOLEAN && !strcmp(option.value, "false"));
  CHECK(p->feature_option_get(NULL, ffb.package_id, ffb.id, 1, &option));
  CHECK(option.type == RECOMP_MOD_OPTION_CHOICE && option.choice_count == 3);
  CHECK(p->feature_option_get(NULL, ffb.package_id, ffb.id, 2, &option));
  CHECK(!strcmp(option.id, "ImpactStrength") && !strcmp(option.value, "20"));
  CHECK(p->feature_option_get(NULL, ffb.package_id, ffb.id, 3, &option));
  CHECK(!strcmp(option.id, "ImpactType") && !strcmp(option.value, "Constant") &&
        option.type == RECOMP_MOD_OPTION_CHOICE && option.choice_count == 2);
  RecompLauncherCModChoice choice;
  CHECK(p->feature_choice_get(NULL, ffb.package_id, ffb.id, "Device", 1, &choice));
  CHECK(!strcmp(choice.value, "MOZA R12 Base"));
  CHECK(p->feature_choice_get(NULL, ffb.package_id, ffb.id, "ImpactType", 1, &choice));
  CHECK(!strcmp(choice.value, "Sine"));
  CHECK(p->feature_set_option(NULL, wheel.package_id, wheel.id,
                              "SteeringRangePercent", "45"));
  CHECK(p->feature_set_option(NULL, wheel.package_id, wheel.id,
                              "ButtonRewind", "37"));
  CHECK(p->feature_set_option(NULL, wheel.package_id, wheel.id,
                              "ButtonUp", "128"));
  CHECK(p->feature_option_get(NULL, wheel.package_id, wheel.id, 17, &option));
  CHECK(!strcmp(option.value, "128"));
  CHECK(!p->feature_set_option(NULL, wheel.package_id, wheel.id,
                               "ButtonUp", "192"));
  CHECK(!p->feature_set_option(NULL, wheel.package_id, wheel.id,
                               "ButtonRewind", "128"));
  CHECK(p->feature_set_option(NULL, ffb.package_id, ffb.id,
                              "Strength", "40"));
  CHECK(p->feature_set_option(NULL, ffb.package_id, ffb.id,
                              "ImpactStrength", "25"));
  CHECK(p->feature_set_option(NULL, ffb.package_id, ffb.id,
                              "ImpactType", "Sine"));
  CHECK(!p->feature_set_option(NULL, ffb.package_id, ffb.id,
                               "ImpactStrength", "101"));
  CHECK(!p->feature_set_option(NULL, ffb.package_id, ffb.id,
                               "ImpactType", "Square"));
  CHECK(p->feature_set_option(NULL, ffb.package_id, ffb.id,
                              "Device", "MOZA R12 Base"));
  CHECK(!p->feature_set_option(NULL, ffb.package_id, ffb.id,
                               "Device", "unknown"));
  CHECK(p->feature_set_option(NULL, wheel.package_id, wheel.id,
                              "BrakeInvert", "true"));
  CHECK(p->commit(NULL, NULL));
  CHECK(wrote_wheel_range == 45 && wrote_rewind == 37 && wrote_ffb == 40 &&
        wrote_impact == 25 && !strcmp(wrote_impact_type, "Sine"));
  CHECK(wrote_brake_invert == 1 && !strcmp(wrote_device, "MOZA R12 Base"));
  remove("test-mods.ini");
  puts("Independent widescreen and presentation FPS plugins passed");
  return 0;
}
