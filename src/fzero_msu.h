#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Compose the user's Conn/Cubear v11 patch after the optional Deluxe image.
 * The original ROM file and built-in Deluxe payload are never modified. */
bool FzeroMsuPrepare(uint8_t **rom, size_t *size, const char *pack, const char *rom_path);
bool FzeroMsuActive(void);
const char *FzeroMsuError(void);
bool FzeroMsuSelectSaveRoot(void);
void FzeroMsuRestoreAudio(const uint8_t *ram);
