#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common_cpu_infra.h"
#include "fzero_video.h"

const RtlGameInfo *FzeroGameInfo(void);
void FzeroBeginDrawing(uint8_t *pixels, size_t pitch);
void FzeroDrawPpuFrame(void);
int FzeroFrameWidth(void);
uint32_t FzeroResumePc(void);
int FzeroLastLleResult(void);
void FzeroSetViewport(FzeroViewport viewport);
void FzeroPresent(double alpha);
void FzeroSetDeferredPresentation(bool deferred);
