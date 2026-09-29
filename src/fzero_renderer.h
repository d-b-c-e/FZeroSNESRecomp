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
/* Offline reference path: same projection, but reads panorama pixels directly
 * instead of using the BG source-tile atlas. Not used by the live presenter. */
bool FzeroRendererDrawTripleSidesDirectSky(uint32_t *output, size_t capacity,
                                           const FzeroTripleRig *rig,
                                           int logical_width);
/* Advances only when DrawTripleSides writes new pixels, not when it returns
 * cached pixels for another host presentation of the same emulated frame. */
uint64_t FzeroRendererTripleSidesVersion(void);
/* Read-only capture diagnostic. OAM count is the number of non-sentinel
 * vehicle-owned slots at scanline 100. Raster sprite pixels count decoded,
 * nontransparent OBJ pixels within a 684-pixel expanded diagnostic viewport
 * across the captured lines when the car projects onto a side panel; neither
 * value proves the car is visible after priority,
 * clipping and colour-window composition. */
typedef struct FzeroTripleVehicleProbe {
  unsigned state, oam_slots, raster_sprite_pixels;
  int world_x, world_y, guest_x, guest_y;
  int raster_left, raster_top, raster_right, raster_bottom;
  bool projected[3];
  double panel_x[3], panel_y[3];
} FzeroTripleVehicleProbe;
bool FzeroRendererProbeTripleVehicles(const FzeroTripleRig *rig,
                                      int logical_width,
                                      FzeroTripleVehicleProbe out[6]);
/* Offline-only side-car placement preview. Decodes only captured, owner-tagged
 * OBJ art and projects it as a center-facing billboard at the calibrated
 * ground anchor. This intentionally does not run in the game presenter. */
bool FzeroRendererPreviewTripleVehicles(uint32_t *sides, size_t capacity,
                                        const FzeroTripleRig *rig,
                                        int logical_width,
                                        unsigned *written_pixels);
