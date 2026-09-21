#pragma once

#include "fzero_video.h"
#include "snes/ppu.h"

void FzeroRendererReset(void);
void FzeroRendererBeginFrame(const uint8_t ram[0x20000], unsigned frame);
void FzeroRendererCaptureLine(const Ppu *ppu, unsigned line);
void FzeroRendererEndFrame(const Ppu *ppu, const uint32_t stock[256 * 224]);
bool FzeroRendererDraw(uint32_t *output, FzeroViewport viewport, double alpha);
/* capacity is in pixels. Native UI/OBJ stay crisp; only Mode 7 is resampled. */
bool FzeroRendererDrawHd(uint32_t *output, size_t capacity,
                         FzeroViewport viewport, double alpha, unsigned scale);
bool FzeroRendererHasFrame(void);
bool FzeroRendererLoadCapture(const char *path);
const uint32_t *FzeroRendererStockFrame(void);
