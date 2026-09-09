#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "fzero_video.h"

bool FzeroDlssStart(void);
void FzeroDlssConfigure(const FzeroVideoSettings *settings);
void FzeroDlssReset(void);
const char *FzeroDlssStatus(void);
const uint32_t *FzeroDlssOriginal(void);
const uint32_t *FzeroDlssFrame(const uint32_t *pixels, int width, int height,
                              double aspect, int *out_width, int *out_height);
void FzeroDlssStop(void);
