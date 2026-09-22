#include "fzero_gamepad.h"
#include "fzero_analog.h"
#include "fzero_hotkeys.h"

#include <stdio.h>
#include <string.h>

static char s_config[1024], s_preferred[40];
static uint32_t s_bind[12];
static int s_default_deadzone, s_deadzone;
static bool s_analog_steering;
static FzeroAnalogSteering s_steering;
static SDL_Joystick *s_raw;
static int s_raw_axis = 0, s_gas_axis = -1, s_brake_axis = -1;
static int s_gas_invert, s_brake_invert, s_pedal_threshold;
static int s_raw_buttons[8];
static const unsigned s_raw_input_bits[8] = {8, 0, 9, 1, 10, 11, 2, 3};

/* SDL's standard button indices are shared by SDL2 and SDL3. Triggers are
 * axes, represented here by bits 15/16 after the standard fifteen buttons. */
static const char *const s_names[] = {
    "A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3",
    "Lb", "Rb", "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "L2", "R2"};
static const unsigned s_input_bits[12] = {4, 5, 6, 7, 2, 3, 8, 0, 9, 1, 10, 11};

static void pad_guid(SDL_GameController *pad, char guid[40]) {
#if SNESRECOMP_SDL3
  SDL_GUIDToString(SDL_GetJoystickGUID(SDL_GetGamepadJoystick(pad)), guid, 40);
#else
  SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(pad)), guid, 40);
#endif
}

static void joystick_guid(SDL_Joystick *stick, char guid[40]) {
#if SNESRECOMP_SDL3
  SDL_GUIDToString(SDL_GetJoystickGUID(stick), guid, 40);
#else
  SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(stick), guid, 40);
#endif
}

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t') ++s;
  size_t n = strlen(s);
  while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r')) s[--n] = 0;
  return s;
}

static uint32_t binding(char *text) {
  uint32_t mask = 0;
  for (char *part = text; part;) {
    char *next = strchr(part, '+');
    if (next) *next++ = 0;
    part = trim(part);
    int button = -1;
    if (!SDL_strcasecmp(part, "L1")) button = 9;
    if (!SDL_strcasecmp(part, "R1")) button = 10;
    for (int i = 0; i < 17; ++i)
      if (!SDL_strcasecmp(part, s_names[i])) button = i;
    if (button < 0) return 0; /* empty/unbound/unknown stays unbound */
    mask |= 1u << button;
    part = next;
  }
  return mask;
}

static void load_profile(SDL_GameController *pad) {
  char guid[40], section[64];
  pad_guid(pad, guid);
  snprintf(section, sizeof(section), "Controller.%s", guid);
  char controls[1024] = "DpadUp,DpadDown,DpadLeft,DpadRight,Back,Start,B,A,Y,X,Lb,Rb";
  FzeroIniReadString(s_config, "GamepadMap", "Controls", controls, sizeof(controls));
  FzeroIniReadString(s_config, section, "Controls", controls, sizeof(controls));
  char *comment = strpbrk(controls, "#;");
  if (comment) *comment = 0;
  memset(s_bind, 0, sizeof(s_bind));
  char *part = controls;
  for (int i = 0; i < 12 && part; ++i) {
    char *next = strchr(part, ',');
    if (next) *next++ = 0;
    s_bind[i] = binding(part);
    part = next;
  }
  int percent = s_default_deadzone;
  FzeroIniReadInt(s_config, section, "Deadzone", &percent);
  if (percent < 0 || percent > 100) percent = s_default_deadzone;
  s_deadzone = (percent * 32767 + 50) / 100;
  int analog = 0;
  FzeroIniReadInt(s_config, section, "AnalogSteering", &analog);
  s_analog_steering = analog != 0;
  FzeroAnalogSteeringReset(&s_steering);
  fprintf(stderr, "[fzero-input] %s guid=%s deadzone=%d%% steering=%s\n",
          SDL_GameControllerName(pad), guid, percent,
          s_analog_steering ? "analog" : "digital");
}

static void load_raw_profile(SDL_Joystick *stick) {
  char guid[40], section[64];
  joystick_guid(stick, guid);
  snprintf(section, sizeof(section), "Controller.%s", guid);
  int percent = s_default_deadzone, analog = 1;
  FzeroIniReadInt(s_config, section, "Deadzone", &percent);
  if (percent < 0 || percent > 100) percent = s_default_deadzone;
  s_deadzone = (percent * 32767 + 50) / 100;
  FzeroIniReadInt(s_config, section, "AnalogSteering", &analog);
  s_analog_steering = analog != 0;
  s_raw_axis = 0; s_gas_axis = s_brake_axis = -1;
  s_gas_invert = s_brake_invert = 0; s_pedal_threshold = 0;
  FzeroIniReadInt(s_config, section, "SteeringAxis", &s_raw_axis);
  FzeroIniReadInt(s_config, section, "AcceleratorAxis", &s_gas_axis);
  FzeroIniReadInt(s_config, section, "BrakeAxis", &s_brake_axis);
  FzeroIniReadInt(s_config, section, "AcceleratorInvert", &s_gas_invert);
  FzeroIniReadInt(s_config, section, "BrakeInvert", &s_brake_invert);
  FzeroIniReadInt(s_config, section, "PedalThreshold", &s_pedal_threshold);
  const char *keys[8] = {"ButtonA", "ButtonB", "ButtonX", "ButtonY",
                         "ButtonL", "ButtonR", "ButtonSelect", "ButtonStart"};
  for (int i = 0; i < 8; ++i) {
    s_raw_buttons[i] = -1;
    FzeroIniReadInt(s_config, section, keys[i], &s_raw_buttons[i]);
  }
  FzeroAnalogSteeringReset(&s_steering);
  fprintf(stderr, "[fzero-input] raw %s guid=%s steering-axis=%d gas-axis=%d brake-axis=%d\n",
          SDL_JoystickName(stick), guid, s_raw_axis, s_gas_axis, s_brake_axis);
}

void FzeroGamepadConfigure(const char *config, const char *guid, int deadzone) {
  snprintf(s_config, sizeof(s_config), "%s", config ? config : "config.ini");
  snprintf(s_preferred, sizeof(s_preferred), "%s", guid ? guid : "");
  s_default_deadzone = deadzone >= 0 && deadzone <= 100 ? deadzone : 25;
  s_analog_steering = false;
  FzeroAnalogSteeringReset(&s_steering);
}

void FzeroGamepadRefresh(SDL_GameController **pad) {
  if (*pad && !SDL_GameControllerGetAttached(*pad)) {
    SDL_GameControllerClose(*pad);
    *pad = NULL;
    FzeroAnalogSteeringReset(&s_steering);
  }
  if (*pad) return;
  if (s_raw && !SDL_JoystickGetAttached(s_raw)) {
    SDL_JoystickClose(s_raw);
    s_raw = NULL;
  }
  if (s_raw) return;
#if SNESRECOMP_SDL3
  int count = 0;
  SDL_JoystickID *ids = SDL_GetGamepads(&count);
#else
  int count = SDL_NumJoysticks();
#endif
  for (int i = 0; i < count && !*pad; ++i) {
#if SNESRECOMP_SDL3
    SDL_GameController *candidate = SDL_OpenGamepad(ids[i]);
#else
    if (!SDL_IsGameController(i)) continue;
    SDL_GameController *candidate = SDL_GameControllerOpen(i);
#endif
    if (!candidate) continue;
    char guid[40];
    pad_guid(candidate, guid);
    if (s_preferred[0] && SDL_strcasecmp(guid, s_preferred)) {
      SDL_GameControllerClose(candidate);
      continue;
    }
    *pad = candidate;
    load_profile(*pad);
  }
#if SNESRECOMP_SDL3
  SDL_free(ids);
#endif
  if (*pad) return;
#if SNESRECOMP_SDL3
  count = 0;
  ids = SDL_GetJoysticks(&count);
#else
  count = SDL_NumJoysticks();
#endif
  for (int i = 0; i < count && !s_raw; ++i) {
#if SNESRECOMP_SDL3
    SDL_Joystick *candidate = SDL_OpenJoystick(ids[i]);
#else
    SDL_Joystick *candidate = SDL_JoystickOpen(i);
#endif
    if (!candidate) continue;
    char guid[40];
    joystick_guid(candidate, guid);
    if (!s_preferred[0] || SDL_strcasecmp(guid, s_preferred)) {
      if (s_preferred[0])
        fprintf(stderr, "[fzero-input] ignoring raw %s guid=%s (wanted %s)\n",
                SDL_JoystickName(candidate), guid, s_preferred);
      SDL_JoystickClose(candidate);
      continue;
    }
    s_raw = candidate;
    load_raw_profile(s_raw);
  }
#if SNESRECOMP_SDL3
  SDL_free(ids);
#endif
}

void FzeroGamepadEvent(SDL_GameController **pad, const SDL_Event *event) {
  if (event->type == SDL_CONTROLLERDEVICEREMOVED && *pad &&
      SNESRECOMP_SDL_EVENT_DEVICE(*event) ==
          SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*pad))) {
    SDL_GameControllerClose(*pad);
    *pad = NULL;
    FzeroAnalogSteeringReset(&s_steering);
  }
  if (event->type == SDL_CONTROLLERDEVICEADDED ||
      event->type == SDL_CONTROLLERDEVICEREMOVED)
    FzeroGamepadRefresh(pad);
  if (event->type == SDL_CONTROLLERDEVICEREMAPPED && *pad &&
      SNESRECOMP_SDL_EVENT_DEVICE(*event) ==
          SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*pad)))
    load_profile(*pad);
  if (s_raw && !SDL_JoystickGetAttached(s_raw)) {
    SDL_JoystickClose(s_raw);
    s_raw = NULL;
    FzeroAnalogSteeringReset(&s_steering);
    FzeroGamepadRefresh(pad);
  }
}

uint32_t FzeroGamepadRead(SDL_GameController *pad) {
  if ((!pad || !SDL_GameControllerGetAttached(pad)) &&
      (!s_raw || !SDL_JoystickGetAttached(s_raw))) return 0;
  if (!pad) {
    uint32_t input = 0;
    int x = SDL_JoystickGetAxis(s_raw, s_raw_axis);
    if (s_analog_steering)
      input |= FzeroAnalogSteeringRead(&s_steering, x, s_deadzone);
    else {
      if (x < -s_deadzone) input |= 0x0040u;
      if (x > s_deadzone) input |= 0x0080u;
    }
    if (s_gas_axis >= 0) {
      int value = SDL_JoystickGetAxis(s_raw, s_gas_axis);
      if (s_gas_invert) value = -value;
      if (value > s_pedal_threshold) input |= 0x0001u;
    }
    if (s_brake_axis >= 0) {
      int value = SDL_JoystickGetAxis(s_raw, s_brake_axis);
      if (s_brake_invert) value = -value;
      if (value > s_pedal_threshold) input |= 0x0002u;
    }
    for (int i = 0; i < 8; ++i)
      if (s_raw_buttons[i] >= 0 && SDL_JoystickGetButton(s_raw, s_raw_buttons[i]))
        input |= 1u << s_raw_input_bits[i];
    return input;
  }
  uint32_t buttons = 0, input = 0;
  for (int i = 0; i < 15; ++i)
    if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)i)) buttons |= 1u << i;
  if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > s_deadzone)
    buttons |= 1u << 15;
  if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > s_deadzone)
    buttons |= 1u << 16;
  for (int i = 0; i < 12; ++i)
    if (s_bind[i] && (buttons & s_bind[i]) == s_bind[i]) input |= 1u << s_input_bits[i];
  int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
  int y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
  if (s_analog_steering)
    input |= FzeroAnalogSteeringRead(&s_steering, x, s_deadzone);
  else {
    if (x < -s_deadzone) input |= 0x0040u;
    if (x > s_deadzone) input |= 0x0080u;
  }
  if (y < -s_deadzone) input |= 0x0010u;
  if (y > s_deadzone) input |= 0x0020u;
  return input;
}

void FzeroGamepadShutdown(SDL_GameController **pad) {
  if (pad && *pad) {
    SDL_GameControllerClose(*pad);
    *pad = NULL;
  }
  if (s_raw) {
    SDL_JoystickClose(s_raw);
    s_raw = NULL;
  }
  FzeroAnalogSteeringReset(&s_steering);
}
