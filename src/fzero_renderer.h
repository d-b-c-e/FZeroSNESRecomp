#pragma once

#include "fzero_video.h"
#include "fzero_triple_geometry.h"
#include "snes/ppu.h"

void FzeroRendererReset(void);
void FzeroRendererBeginFrame(const uint8_t ram[0x20000], unsigned frame);
void FzeroRendererCaptureLine(const Ppu *ppu, unsigned line);
void FzeroRendererEndFrame(const Ppu *ppu, const uint32_t stock[256 * 224]);
bool FzeroRendererDraw(uint32_t *output, FzeroViewport viewport, double alpha);
/* capacity is in pixels. Native UI/OBJ stay crisp; only Mode 7 is resampled. */
bool FzeroRendererDrawHd(uint32_t *output, size_t capacity,
                         FzeroViewport viewport, double alpha, unsigned scale);
#define FZERO_RENDERER_COMBINED_PRESENTATION 1
/* Produce the native thumbnail/rewind frame and HD presentation together.
 * native may be NULL; otherwise it holds width*224 pixels. Buffers must not
 * overlap. hd_capacity is in pixels. Native composition stays exact. */
bool FzeroRendererDrawPresentation(uint32_t *native, uint32_t *hd, size_t hd_capacity,
                                   FzeroViewport viewport, double alpha, unsigned scale);
bool FzeroRendererHasFrame(void);
bool FzeroRendererLoadCapture(const char *path);
const uint32_t *FzeroRendererStockFrame(void);
/* Experimental live Mode 7 side panels. Output is two contiguous ARGB panels,
 * each panel_width*panel_height pixels. False means draw the stock fallback. */
bool FzeroRendererDrawTripleSides(uint32_t *output, size_t capacity,
                                  const FzeroTripleRig *rig, int logical_width);
/* Read-only capture diagnostic. OAM count is the number of non-sentinel
 * vehicle-owned slots at scanline 100, not proof that side artwork exists. */
typedef struct FzeroTripleVehicleProbe {
  unsigned state, oam_slots;
  int world_x, world_y, guest_x, guest_y;
  bool projected[3];
  double panel_x[3], panel_y[3];
} FzeroTripleVehicleProbe;
bool FzeroRendererProbeTripleVehicles(const FzeroTripleRig *rig,
                                      int logical_width,
                                      FzeroTripleVehicleProbe out[6]);
