#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Launch-time selection; immutable while the guest is running. */
bool FzeroDeluxeActive(void);
bool FzeroDeluxePrepare(uint8_t **rom, size_t *size, bool enabled, const char *path);
const char *FzeroDeluxeError(void);
bool FzeroDeluxeSelectSaveRoot(void);
