#include "fzero_mods.h"
#include "fzero_hotkeys.h"
#include "raw_hat_binding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FzeroVideoSettings *video;
static const char *config_path;
static char error_text[128];
static const char *const aspects[] = {"16:9", "21:9", "32:9", "Fit"};
static const char *const rates[] = {"Auto", "60", "90", "120", "144", "165", "240", "360"};
#define COPY(field, value) snprintf(field, sizeof(field), "%s", value)
static const char *const packages[] = {"fzero-widescreen", "fzero-presentation-fps", "bs-deluxe", "fzero-hd-mode7", "fzero-diagnostics", "fzero-wheel", "fzero-ffb"};
static const char *const features[] = {"widescreen", "presentation-fps", "bs-deluxe", "hd-mode7", "diagnostics", "wheel", "force-feedback"};
static const char *const names[] = {"Widescreen", "Presentation FPS", "BS Deluxe", "HD Mode 7", "Diagnostics", "Racing wheel controls", "Force feedback"};
static const char *const descriptions[] = {
  "Expand the race view and anchor the HUD at its outer edges.",
  "Choose the presentation rate independently of widescreen and game speed.",
  "Full BS Deluxe v1.1: original and BS courses, eight vehicles, alternate cups and Practice ghosts. Uses separate saves.",
  "Render the track at higher resolution with smoother scanline geometry. Works independently of widescreen and presentation FPS.",
  "Record hardware, active video settings and frame timings in the diagnostics folder beside the game (beside the AppImage on Linux). Off by default. Enable, play through a slowdown, then attach the newest performance JSONL file to your report. Logs stay on your machine; no ROM or save data is included.",
  "Tune wheel steering, pedals, SNES buttons and host save-state/rewind buttons. Button indices are zero-based SDL joystick buttons.",
  "Speed-dependent centering, damping, road texture and collision impulses on the chosen wheel."
};
typedef struct WheelOption { const char *key, *label; int fallback, min, max; } WheelOption;
static const WheelOption wheel_options[] = {
  {"Deadzone", "Center dead zone (%)", 0, 0, 20},
  {"SteeringRangePercent", "Travel to full steering (%)", 100, 10, 100},
  {"SteeringResponsePercent", "Response curve (%)", 50, 25, 200},
  {"SteeringAxis", "Steering axis", 0, 0, 15},
  {"AcceleratorAxis", "Accelerator axis", -1, -1, 15},
  {"BrakeAxis", "Brake axis", -1, -1, 15},
  {"PedalThreshold", "Pedal activation threshold", 0, -32768, 32767},
  {"AcceleratorInvert", "Invert accelerator", 0, 0, 1},
  {"BrakeInvert", "Invert brake", 0, 0, 1},
  {"ButtonA", "A / boost", -1, -1, 127},
  {"ButtonB", "B / accelerate", -1, -1, 127},
  {"ButtonX", "X", -1, -1, 127},
  {"ButtonY", "Y / brake", -1, -1, 127},
  {"ButtonL", "L shoulder", -1, -1, 127},
  {"ButtonR", "R shoulder", -1, -1, 127},
  {"ButtonSelect", "Select", -1, -1, 127},
  {"ButtonStart", "Start", -1, -1, 127},
  {"ButtonUp", "D-pad up", -1, -1, RECOMP_RAW_HAT_MAX},
  {"ButtonDown", "D-pad down", -1, -1, RECOMP_RAW_HAT_MAX},
  {"ButtonLeft", "D-pad left", -1, -1, RECOMP_RAW_HAT_MAX},
  {"ButtonRight", "D-pad right", -1, -1, RECOMP_RAW_HAT_MAX},
  {"ButtonSaveStateMenu", "Open save-state menu", -1, -1, 127},
  {"ButtonRewind", "Open rewind", -1, -1, 127}
};
#define WHEEL_OPTIONS ((int)(sizeof(wheel_options) / sizeof(wheel_options[0])))
static int wheel_values[WHEEL_OPTIONS], wheel_enabled, ffb_enabled, ffb_strength;
static char ffb_device[256], wheel_section[64];
static char ffb_devices[16][256];
static int ffb_device_count;
static const char *wheel_config;
static void (*wheel_write)(const char *, const char *, const char *, const char *);
static int count(void *ctx) { (void)ctx; return wheel_config ? 7 : 5; }
static int identity(const char *package, const char *feature) {
  if (package && feature) for (int i = 0; i < count(NULL); ++i)
    if (!strcmp(package, packages[i]) && !strcmp(feature, features[i])) return i + 1;
  return 0;
}
static int package_get(void *ctx, int index, RecompLauncherCModPackage *out) {
  (void)ctx;
  if (index < 0 || index >= count(NULL) || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, packages[index]); COPY(out->version, "1");
  COPY(out->name, names[index]); COPY(out->author, index == 2 ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 6 ? ffb_enabled : index == 5 ? wheel_enabled : index == 4 ? video->diagnostics : index == 3 ? video->hd_mode7 : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  return 1;
}
static int feature_get(void *ctx, int index, RecompLauncherCModFeature *out) {
  (void)ctx;
  if (index < 0 || index >= count(NULL) || !out) return 0;
  memset(out, 0, sizeof(*out));
  COPY(out->id, features[index]); COPY(out->package_id, packages[index]);
  COPY(out->package_name, names[index]); COPY(out->package_version, "1");
  COPY(out->name, names[index]); COPY(out->group, index >= 5 ? "Controls" : index == 4 ? "Support" : index == 2 ? "Content" : "Presentation");
  COPY(out->author, index == 2 ? "GuyPerfect, PowerPanda, Porthor, Catador" : "FZeroSNESRecomp contributors");
  COPY(out->description, descriptions[index]);
  out->enabled = index == 6 ? ffb_enabled : index == 5 ? wheel_enabled : index == 4 ? video->diagnostics : index == 3 ? video->hd_mode7 : index == 2 ? video->bs_deluxe : index ? video->fps_enabled : video->enhanced;
  COPY(out->status, out->enabled ? "Enabled" : "Disabled");
  if (index == 3 && video->hd_scale > 4) {
    snprintf(out->description, sizeof(out->description),
        "%s\n\nWarning: %ux is extremely demanding and can cause severe slowdown, "
        "especially with ultrawide views or high Presentation FPS. Use at your own risk. "
        "Try 2x and 60 FPS if performance drops.", descriptions[index], video->hd_scale);
    COPY(out->status, "Warning: high CPU and memory use above 4x");
  }
  out->option_count = index == 5 ? WHEEL_OPTIONS : index == 6 ? 2 : index == 2 || index == 4 ? 0 : 1;
  return 1;
}
static int option_get(void *ctx, const char *package, const char *feature, int index,
                      RecompLauncherCModOption *out) {
  (void)ctx;
  int kind = identity(package, feature);
  if (!kind || kind == 3 || kind == 5 || !out) return 0;
  if (kind == 6 && index >= 0 && index < WHEEL_OPTIONS) {
    const WheelOption *spec = &wheel_options[index];
    memset(out, 0, sizeof(*out));
    COPY(out->id, spec->key); COPY(out->label, spec->label);
    out->type = RECOMP_MOD_OPTION_INTEGER; out->step = 1;
    if (index == 7 || index == 8) {
      out->type = RECOMP_MOD_OPTION_BOOLEAN;
    } else if (index >= 3 && index <= 5) {
      out->type = RECOMP_MOD_OPTION_RAW_AXIS;
      COPY(out->device_guid, wheel_section + strlen("Controller."));
    } else if (index >= 9) {
      out->type = RECOMP_MOD_OPTION_RAW_BUTTON;
      COPY(out->device_guid, wheel_section + strlen("Controller."));
    }
    out->min_value = spec->min; out->max_value = spec->max;
    if (out->type == RECOMP_MOD_OPTION_BOOLEAN) {
      COPY(out->value, wheel_values[index] ? "true" : "false");
      COPY(out->default_value, "false");
    } else {
      snprintf(out->value, sizeof(out->value), "%d", wheel_values[index]);
      snprintf(out->default_value, sizeof(out->default_value), "%d", spec->fallback);
    }
    return 1;
  }
  if (kind == 7 && index >= 0 && index < 2) {
    memset(out, 0, sizeof(*out));
    COPY(out->id, index ? "Device" : "Strength");
    COPY(out->label, index ? "FFB device" : "Strength (%)");
    out->type = index ? RECOMP_MOD_OPTION_CHOICE : RECOMP_MOD_OPTION_INTEGER;
    out->choice_count = index ? ffb_device_count + 1 : 0;
    out->min_value = 0; out->max_value = 100; out->step = 1;
    if (index) { COPY(out->value, ffb_device); COPY(out->default_value, ""); }
    else { snprintf(out->value, sizeof(out->value), "%d", ffb_strength); COPY(out->default_value, "35"); }
    return 1;
  }
  if (index != 0) return 0;
  memset(out, 0, sizeof(*out)); out->type = RECOMP_MOD_OPTION_CHOICE; out->step = 1;
  if (kind == 1) {
    COPY(out->id, "aspect"); COPY(out->label, "Aspect ratio");
    COPY(out->description, "Fit follows the window from 4:3 through 32:9.");
    COPY(out->value, FzeroAspectName(video->aspect));
    /* Must track FzeroVideoDefaults, or the launcher marks the wrong choice. */
    COPY(out->default_value, FzeroAspectName(FZERO_ASPECT_FIT));
    out->choice_count = 4;
  } else if (kind == 4) {
    COPY(out->id, "scale"); COPY(out->label, "Resolution multiplier (2-10)");
    COPY(out->description, "Whole numbers from 2 to 10 per dimension. 2x recommended; above 4x can cause severe slowdown. Use at your own risk.");
    snprintf(out->value, sizeof(out->value), "%u", video->hd_scale);
    COPY(out->default_value, "2");
    out->type = RECOMP_MOD_OPTION_INTEGER;
    out->min_value = FZERO_HD_SCALE_MIN; out->max_value = FZERO_HD_SCALE_MAX;
  } else {
    COPY(out->id, "fps"); COPY(out->label, "Presentation FPS");
    COPY(out->description, "Auto follows display refresh, up to 360 FPS.");
    if (video->fps) snprintf(out->value, sizeof(out->value), "%u", video->fps);
    else COPY(out->value, "Auto");
    COPY(out->default_value, "Auto"); out->choice_count = 8;
  }
  return 1;
}
static int choice_get(void *ctx, const char *package, const char *feature,
                      const char *option, int index, RecompLauncherCModChoice *out) {
  (void)ctx;
  if (!identity(package, feature) || !option || !out || index < 0) return 0;
  const char *value = NULL;
  if (identity(package, feature) == 7 && !strcmp(option, "Device") &&
      index <= ffb_device_count) {
    memset(out, 0, sizeof(*out));
    if (index == 0) {
      COPY(out->label, "No device selected");
    } else {
      COPY(out->value, ffb_devices[index - 1]);
      COPY(out->label, ffb_devices[index - 1]);
    }
    return 1;
  }
  if (identity(package, feature) == 1 && !strcmp(option, "aspect") && index < 4) value = aspects[index];
  if (identity(package, feature) == 2 && !strcmp(option, "fps") && index < 8) value = rates[index];
  if (!value) return 0;
  memset(out, 0, sizeof(*out)); COPY(out->value, value);
  COPY(out->label, !strcmp(value, "Fit") ? "Fit to window" : value);
  return 1;
}
static int enable(void *ctx, const char *package, const char *feature, int enabled) {
  (void)ctx;
  if (!identity(package, feature)) return 0;
  if (identity(package, feature) == 7) ffb_enabled = enabled != 0;
  else if (identity(package, feature) == 6) wheel_enabled = enabled != 0;
  else if (identity(package, feature) == 5) video->diagnostics = enabled != 0;
  else if (identity(package, feature) == 4) video->hd_mode7 = enabled != 0;
  else if (identity(package, feature) == 3) video->bs_deluxe = enabled != 0;
  else if (identity(package, feature) == 2) video->fps_enabled = enabled != 0;
  else {
    video->enhanced = enabled != 0;
    if (video->aspect == FZERO_ASPECT_STOCK) video->aspect = FZERO_ASPECT_16_9;
  }
  return 1;
}
static int set_option(void *ctx, const char *package, const char *feature,
                      const char *option, const char *value) {
  (void)ctx;
  if (!identity(package, feature) || !option || !value) return 0;
  if (identity(package, feature) == 6 || identity(package, feature) == 7) {
    if (identity(package, feature) == 7 && !strcmp(option, "Device")) {
      if (strlen(value) >= sizeof(ffb_device)) return 0;
      int found = !value[0];
      for (int i = 0; i < ffb_device_count; ++i)
        if (!strcmp(value, ffb_devices[i])) found = 1;
      if (!found) return 0;
      COPY(ffb_device, value); return 1;
    }
    if (identity(package, feature) == 6 &&
        (!strcmp(option, "AcceleratorInvert") ||
         !strcmp(option, "BrakeInvert"))) {
      int index = !strcmp(option, "AcceleratorInvert") ? 7 : 8;
      if (strcmp(value, "true") && strcmp(value, "false")) return 0;
      wheel_values[index] = !strcmp(value, "true");
      return 1;
    }
    char *end;
    long parsed = strtol(value, &end, 10);
    if (!value[0] || *end) return 0;
    if (identity(package, feature) == 7) {
      if (strcmp(option, "Strength") || parsed < 0 || parsed > 100) return 0;
      ffb_strength = (int)parsed; return 1;
    }
    for (int i = 0; i < WHEEL_OPTIONS; ++i) {
      const WheelOption *spec = &wheel_options[i];
      if (strcmp(option, spec->key)) continue;
      if (parsed < spec->min || parsed > spec->max) return 0;
      wheel_values[i] = (int)parsed; return 1;
    }
    return 0;
  }
  if (identity(package, feature) == 1 && !strcmp(option, "aspect")) {
    for (unsigned i = 0; i < 4; ++i) if (!strcmp(value, aspects[i]))
      return FzeroParseAspect(value, &video->aspect);
  } else if (identity(package, feature) == 4 && !strcmp(option, "scale")) {
    if (FzeroParseHdScale(value, &video->hd_scale)) { error_text[0] = 0; return 1; }
    COPY(error_text, "HD Mode 7 resolution must be a whole number from 2 to 10.");
  } else if (identity(package, feature) == 2 && !strcmp(option, "fps")) {
    for (unsigned i = 0; i < 8; ++i) if (!strcmp(value, rates[i])) {
      video->fps = i ? (unsigned)atoi(value) : 0; return 1;
    }
  }
  return 0;
}
static int commit(void *ctx, const char *image) {
  (void)ctx; (void)image;
  error_text[0] = 0;
  if (FzeroVideoSave(video, config_path)) {
    if (wheel_config && wheel_write) {
      char value[32];
      snprintf(value, sizeof(value), "%d", wheel_enabled);
      wheel_write(wheel_config, wheel_section, "AnalogSteering", value);
      for (int i = 0; i < WHEEL_OPTIONS; ++i) {
        snprintf(value, sizeof(value), "%d", wheel_values[i]);
        wheel_write(wheel_config, wheel_section, wheel_options[i].key, value);
      }
      snprintf(value, sizeof(value), "%d", ffb_enabled);
      wheel_write(wheel_config, "ForceFeedback", "Enabled", value);
      snprintf(value, sizeof(value), "%d", ffb_strength);
      wheel_write(wheel_config, "ForceFeedback", "Strength", value);
      wheel_write(wheel_config, "ForceFeedback", "Device", ffb_device);
    }
    return 1;
  }
  COPY(error_text, "Unable to save fzero-video.ini"); return 0;
}
static const char *last_error(void *ctx) { (void)ctx; return error_text; }

const RecompLauncherCModProvider *FzeroModsProvider(FzeroVideoSettings *settings, const char *path) {
  static RecompLauncherCModProvider provider;
  video = settings; config_path = path; error_text[0] = 0;
  wheel_config = NULL; wheel_write = NULL;
  ffb_device_count = 0;
  memset(&provider, 0, sizeof(provider));
  provider.package_count = count; provider.package_get = package_get;
  provider.feature_count = count; provider.feature_get = feature_get;
  provider.feature_option_get = option_get; provider.feature_choice_get = choice_get;
  provider.feature_enable = enable; provider.feature_set_option = set_option;
  provider.commit = commit; provider.last_error = last_error;
  return &provider;
}

const RecompLauncherCModProvider *FzeroModsProviderWheel(
    FzeroVideoSettings *settings, const char *video_path,
    const char *control_path, const char *wheel_guid,
    void (*write_ini)(const char *, const char *, const char *, const char *),
    int (*list_ffb_devices)(char names[][256], int max_devices)) {
  const RecompLauncherCModProvider *provider = FzeroModsProvider(settings, video_path);
  if (!control_path || !wheel_guid || !wheel_guid[0]) return provider;
  wheel_config = control_path; wheel_write = write_ini;
  if (list_ffb_devices) {
    ffb_device_count = list_ffb_devices(ffb_devices, 16);
    if (ffb_device_count < 0) ffb_device_count = 0;
    if (ffb_device_count > 16) ffb_device_count = 16;
  }
  snprintf(wheel_section, sizeof(wheel_section), "Controller.%s", wheel_guid);
  wheel_enabled = 1; ffb_enabled = 0; ffb_strength = 35;
  ffb_device[0] = 0;
  FzeroIniReadInt(control_path, wheel_section, "AnalogSteering", &wheel_enabled);
  for (int i = 0; i < WHEEL_OPTIONS; ++i) {
    wheel_values[i] = wheel_options[i].fallback;
    FzeroIniReadInt(control_path, wheel_section, wheel_options[i].key,
                    &wheel_values[i]);
  }
  FzeroIniReadInt(control_path, "ForceFeedback", "Enabled", &ffb_enabled);
  FzeroIniReadInt(control_path, "ForceFeedback", "Strength", &ffb_strength);
  FzeroIniReadString(control_path, "ForceFeedback", "Device", ffb_device,
                     sizeof(ffb_device));
  return provider;
}
