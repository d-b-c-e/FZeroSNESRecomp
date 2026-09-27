#pragma once

#include "desktop/sdl_compat.h"

/* Own the gameplay handle independently of the launcher's SDL lifetime. */
void FzeroGamepadConfigure(const char *config, const char *guid, int deadzone);
void FzeroGamepadRefresh(SDL_GameController **pad);
void FzeroGamepadEvent(SDL_GameController **pad, const SDL_Event *event);
uint32_t FzeroGamepadRead(SDL_GameController *pad);
/* Host-only wheel buttons, outside the guest's 12-bit SNES word. */
enum { FZERO_WHEEL_SAVE_MENU = 1u << 24, FZERO_WHEEL_REWIND = 1u << 25 };
void FzeroGamepadShutdown(SDL_GameController **pad);
