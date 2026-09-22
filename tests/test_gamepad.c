#include "fzero_gamepad.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s (%s)\n", __LINE__, #x, SDL_GetError()); exit(1); } } while (0)

#if SNESRECOMP_SDL3
static SDL_JoystickID attach(const char *name) {
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = 15;
  desc.name = name;
  SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
  CHECK(id != 0);
  return id;
}

static void pump(SDL_GameController **pad) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) FzeroGamepadEvent(pad, &event);
}

int main(void) {
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  CHECK(SDL_Init(SDL_INIT_GAMEPAD));
  SDL_JoystickID decoy = attach("unselected controller");
  SDL_JoystickID selected = attach("selected controller");
  SDL_Joystick *joy = SDL_OpenJoystick(selected);
  CHECK(joy);
  char guid[40];
  SDL_GUIDToString(SDL_GetJoystickGUID(joy), guid, sizeof(guid));
  FILE *cfg = fopen("gamepad-test.ini", "w");
  CHECK(cfg);
  fprintf(cfg, "[GamepadMap]\nControls = DpadUp,DpadDown,DpadLeft,DpadRight,Back,Start,B,A,Y,X,Lb,Rb\n"
               "[Controller.%s]\nControls = ,DpadDown,DpadLeft,DpadRight,Back,Start,L2,X,Y,A,Lb,Rb\nDeadzone = 3\n", guid);
  fclose(cfg);
  FzeroGamepadConfigure("gamepad-test.ini", guid, 25);
  SDL_GameController *pad = NULL;
  FzeroGamepadRefresh(&pad);
  CHECK(pad && SDL_GetGamepadID(pad) == selected);
  pump(&pad);
  CHECK(SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_WEST, true));
  pump(&pad);
  CHECK(FzeroGamepadRead(pad) == 1u); /* remapped SNES B */
  CHECK(SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_WEST, false));
  CHECK(SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_UP, true));
  pump(&pad);
  CHECK(FzeroGamepadRead(pad) == 0); /* empty first field stays unbound */
  CHECK(SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_UP, false));
  CHECK(SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTX, 2000));
  CHECK(SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767));
  pump(&pad);
  CHECK(FzeroGamepadRead(pad) == (0x0080u | 0x0100u)); /* profile deadzone + L2 */
  CHECK(SDL_DetachVirtualJoystick(decoy));
  pump(&pad);
  CHECK(pad && SDL_GetGamepadID(pad) == selected); /* other device removal */
  SDL_CloseJoystick(joy);
  CHECK(SDL_DetachVirtualJoystick(selected));
  pump(&pad);
  CHECK(!pad && FzeroGamepadRead(pad) == 0);
  selected = attach("selected controller");
  pump(&pad);
  CHECK(pad && SDL_GetGamepadID(pad) == selected); /* same GUID, new instance */
  FzeroGamepadShutdown(&pad);
  CHECK(SDL_DetachVirtualJoystick(selected));

  SDL_VirtualJoystickDesc wheel_desc;
  SDL_INIT_INTERFACE(&wheel_desc);
  wheel_desc.type = SDL_JOYSTICK_TYPE_WHEEL;
  wheel_desc.naxes = 4;
  wheel_desc.nbuttons = 40;
  wheel_desc.name = "raw racing wheel";
  SDL_JoystickID wheel_id = SDL_AttachVirtualJoystick(&wheel_desc);
  CHECK(wheel_id != 0);
  SDL_Joystick *wheel = SDL_OpenJoystick(wheel_id);
  CHECK(wheel);
  SDL_GUIDToString(SDL_GetJoystickGUID(wheel), guid, sizeof(guid));
  cfg = fopen("gamepad-test.ini", "w");
  CHECK(cfg);
  fprintf(cfg, "[Controller.%s]\nAnalogSteering=1\nDeadzone=3\n"
               "SteeringAxis=0\nAcceleratorAxis=2\nBrakeAxis=3\n"
               "ButtonStart=36\n", guid);
  fclose(cfg);
  FzeroGamepadConfigure("gamepad-test.ini", guid, 25);
  FzeroGamepadRefresh(&pad);
  CHECK(!pad);
  CHECK(SDL_SetJoystickVirtualAxis(wheel, 0, 32767));
  CHECK(SDL_SetJoystickVirtualAxis(wheel, 2, 32767));
  CHECK(SDL_SetJoystickVirtualButton(wheel, 36, true));
  SDL_UpdateJoysticks();
  CHECK(FzeroGamepadRead(pad) == (0x0080u | 0x0001u | 0x0008u));
  FzeroGamepadShutdown(&pad);
  SDL_CloseJoystick(wheel);
  CHECK(SDL_DetachVirtualJoystick(wheel_id));
  SDL_Quit();
  remove("gamepad-test.ini");
  puts("F-Zero gamepad: selection, bindings, deadzone, removal and reconnect passed");
  return 0;
}
#endif
