#include "fzero_renderer.h"
#include "fzero_mode7.h"
#include "fzero_triple_ground.h"
#include "snes/mode7_hd.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FzeroRasterLine {
  uint8_t registers[PPU_SAVESTATE_REGS_SIZE];
  uint16_t palette[256], oam[256];
  uint8_t high_oam[32];
} FzeroRasterLine;

typedef struct FzeroSourceFrame {
  FzeroRasterLine lines[224];
  uint16_t vram[0x8000];
  uint32_t stock[256 * 224];
  uint8_t ram[0x20000];
  unsigned frame;
  bool valid;
} FzeroSourceFrame;

static FzeroSourceFrame frames[2];
static unsigned current;
static Ppu scanout; /* Private renderer scratch; never points at guest state. */
static struct {
  uint32_t *output;
  FzeroTripleRig rig;
  int logical_width;
  bool direct_sky;
  uint64_t version;
  bool valid;
} triple_cache;
/* BG1/BG2 panorama tiles are mostly static while horizontal scroll changes
 * with steering. Cache source pixels in panorama coordinates, not final panel
 * columns. Exact tracked VRAM reads invalidate the atlas when art changes. */
static struct {
  uint16_t *pixels[2];
  size_t capacity[2];
  int rows;
  uint8_t vram_used[0x8000];
  uint16_t vram[0x8000];
  uint8_t registers[80][PPU_SAVESTATE_REGS_SIZE];
  bool valid;
} sky_atlas;
static uint8_t *sky_vram_reads;

bool FzeroRendererLoadCapture(const char *path) {
  triple_cache.valid = false;
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  /* Alternate buffers the way FzeroRendererBeginFrame does, so replaying a
   * sequence offline presents the previous frame to the compositor exactly as
   * a session does and can exercise the interpolated presentation path. */
  current ^= 1;
  bool ok = fread(&frames[current], sizeof(frames[current]), 1, f) == 1 && fgetc(f) == EOF;
  fclose(f);
  frames[current].valid = ok;
  return ok;
}
const uint32_t *FzeroRendererStockFrame(void) { return frames[current].stock; }
bool FzeroRendererHasFrame(void) { return frames[current].valid; }

void FzeroRendererReset(void) {
  frames[0].valid = frames[1].valid = false;
  triple_cache.valid = false;
  sky_atlas.valid = false;
}
void FzeroRendererBeginFrame(const uint8_t ram[0x20000], unsigned frame) {
  triple_cache.valid = false;
  current ^= 1;
  FzeroSourceFrame *f = &frames[current];
  f->valid = false;
  f->frame = frame;
  memcpy(f->ram, ram, sizeof(f->ram));
}
void FzeroRendererCaptureLine(const Ppu *p, unsigned line) {
  if (line < 1 || line > 224) return;
  FzeroRasterLine *l = &frames[current].lines[line - 1];
  memcpy(l->registers, p, sizeof(l->registers));
  memcpy(l->palette, p->cgram, sizeof(l->palette));
  memcpy(l->oam, p->oam, sizeof(l->oam));
  memcpy(l->high_oam, p->highOam, sizeof(l->high_oam));
}

static void dump_frame(const FzeroSourceFrame *f) {
  const char *number = getenv("FZERO_CAPTURE_FRAME");
  const char *numbers = getenv("FZERO_CAPTURE_FRAMES");
  const char *prefix = getenv("FZERO_CAPTURE_PREFIX");
  const char *every_text = getenv("FZERO_CAPTURE_EVERY");
  unsigned every = every_text ? (unsigned)strtoul(every_text, NULL, 10) : 0;
  bool selected = number && strtoul(number, NULL, 10) == f->frame;
  if (every && f->frame % every == 0) selected = true;
  if (numbers) for (const char *p = numbers; *p;) {
    char *end;
    if (strtoul(p, &end, 10) == f->frame) selected = true;
    if (end == p || *end != ',') break;
    p = end + 1;
  }
  if (!selected || !prefix) return;
  char path[1024];
  char numbered_prefix[960];
  if (numbers || every) {
    if (snprintf(numbered_prefix, sizeof(numbered_prefix), "%s-%06u", prefix, f->frame) >= (int)sizeof(numbered_prefix)) return;
    prefix = numbered_prefix;
  }
  if (snprintf(path, sizeof(path), "%s.json", prefix) >= (int)sizeof(path)) return;
  FILE *out = fopen(path, "w");
  if (!out) return;
  fprintf(out, "{\"frame\":%u,\"state\":[%u,%u,%u],\"mode7\":%u,\"lines\":[",
          f->frame, f->ram[0x54], f->ram[0x55], f->ram[0x56], f->ram[0x81]);
  for (int y = 0; y < 224; ++y) {
    memcpy(&scanout, f->lines[y].registers, PPU_SAVESTATE_REGS_SIZE);
    fprintf(out, "%s{\"y\":%d,\"mode\":%u,\"brightness\":%u,\"main\":%u,\"sub\":%u,\"math\":%u,\"cgwsel\":%u,\"fixed\":%u,\"matrix\":[",
            y ? "," : "", y, scanout.bgmode, scanout.inidisp, scanout.screenEnabled[0],
            scanout.screenEnabled[1], scanout.cgadsub, scanout.cgwsel, scanout.fixedColor);
    for (int j = 0; j < 8; ++j) fprintf(out, "%s%d", j ? "," : "", scanout.m7matrix[j]);
    fprintf(out, "],\"bgsc\":[%u,%u,%u,%u],\"tileadr\":%u,\"scroll\":[%u,%u,%u,%u,%u,%u,%u,%u]}",
            scanout.bgXsc[0], scanout.bgXsc[1], scanout.bgXsc[2], scanout.bgXsc[3], scanout.bgTileAdr,
            scanout.hScroll[0], scanout.vScroll[0], scanout.hScroll[1], scanout.vScroll[1],
            scanout.hScroll[2], scanout.vScroll[2], scanout.hScroll[3], scanout.vScroll[3]);
  }
  fprintf(out, "],\"oam\":[");
  const FzeroRasterLine *l = &f->lines[100];
  for (int i = 0; i < 128; ++i) {
    unsigned word = l->oam[i * 2], attr = l->oam[i * 2 + 1];
    unsigned high = l->high_oam[i / 4] >> ((i % 4) * 2);
    fprintf(out, "%s[%d,%u,%u,%u,%u]", i ? "," : "", i, (word & 255) | ((high & 1) << 8), word >> 8, attr, (high >> 1) & 1);
  }
  fputs("]}\n", out);
  fclose(out);
  snprintf(path, sizeof(path), "%s.bin", prefix);
  out = fopen(path, "wb");
  if (out) { fwrite(f, sizeof(*f), 1, out); fclose(out); }
}

void FzeroRendererEndFrame(const Ppu *p, const uint32_t stock[256 * 224]) {
  FzeroSourceFrame *f = &frames[current];
  memcpy(f->vram, p->vram, sizeof(f->vram));
  memcpy(f->stock, stock, sizeof(f->stock));
  f->valid = true;
  dump_frame(f);
}

static unsigned tile_pixel(const uint16_t *vram, unsigned address, int x, int y, int bpp) {
  unsigned a = (address + y) & 0x7fff;
  if (sky_vram_reads) {
    sky_vram_reads[a] = 1;
    if (bpp == 4) sky_vram_reads[(a + 8) & 0x7fff] = 1;
  }
  unsigned shift = 7 - x;
  unsigned bits = vram[a] >> shift;
  unsigned pixel = (bits & 1) | ((bits >> 7) & 2);
  if (bpp == 4) {
    bits = vram[(a + 8) & 0x7fff] >> shift;
    pixel |= ((bits & 1) << 2) | ((bits >> 5) & 8);
  }
  return pixel;
}

static uint16_t background_pixel(const Ppu *p, const uint16_t *vram,
                                 int layer, int x, int y, bool extend_panorama) {
  int size = PPU_bigTiles(p, layer) ? 16 : 8;
  int px = (x + p->hScroll[layer]) & 1023, py = (y + p->vScroll[layer]) & 1023;
  /* $A60C packs the skyline into overlapping 512x56 strips, selected by
   * vertical scroll ($A69F: 36,92,148,204). BG1's panorama is 896 pixels;
   * BG2's is 768 and starts at scroll 92. Only the stock 256-pixel view is
   * guaranteed valid in each strip, including the partially filled last one.
   * In the margins, address the full panorama through each strip's first
   * 256 pixels instead of wrapping X into unrelated/padded strip content.
   * Keep this local to the known world layout; HUD and guest VRAM stay intact. */
  if (extend_panorama && layer < 2 && size == 8 &&
      p->bgXsc[layer] == (layer == 0 ? 0x79 : 0x71) &&
      p->hScroll[layer] < 256 && y >= 1 && y < 52) {
    int first = layer == 0 ? 36 : 92;
    int scroll = p->vScroll[layer];
    if (scroll >= first && scroll <= 204 && (scroll - first) % 56 == 0) {
      int band = (scroll - first) / 56;
      int period = layer == 0 ? 896 : 768;
      int panorama_x = (band * 256 + p->hScroll[layer] + x) % period;
      if (panorama_x < 0) panorama_x += period;
      px = panorama_x % 256;
      py = y + first + (panorama_x / 256) * 56;
    }
  }
  int tx = px / size, ty = py / size;
  unsigned sc = p->bgXsc[layer];
  unsigned address = (sc & 0xfc) * 256 + (tx & 31) + (ty & 31) * 32;
  if ((sc & 1) && (tx & 32)) address += 1024;
  if ((sc & 2) && (ty & 32)) address += (sc & 1) ? 2048 : 1024;
  if (sky_vram_reads) sky_vram_reads[address & 0x7fff] = 1;
  unsigned tile = vram[address & 0x7fff];
  int cx = px % size, cy = py % size;
  if (tile & 0x4000) cx = size - 1 - cx;
  if (tile & 0x8000) cy = size - 1 - cy;
  unsigned number = ((tile & 1023) + cx / 8 + (cy / 8) * 16) & 1023;
  int bpp = layer == 2 ? 2 : 4;
  unsigned base = ((p->bgTileAdr >> (layer * 4)) & 15) * 4096;
  unsigned pixel = tile_pixel(vram, base + number * (bpp * 4), cx & 7, cy & 7, bpp);
  if (!pixel) return 0;
  static const unsigned low[] = {8, 7, 1}, high[] = {12, 11, 3};
  unsigned priority = tile & 0x2000 ? high[layer] : low[layer];
  if (layer == 2 && (tile & 0x2000) && (p->bgmode & 8)) priority = 15;
  unsigned palette = ((tile >> 10) & 7) * (1u << bpp);
  return (priority << 12) | (layer << 8) | palette | pixel;
}

static bool in_window(const Ppu *p, int layer, int x, int extra) {
  unsigned flags = (p->windowsel >> (layer * 4)) & 15;
  bool enabled1 = (flags & 2) != 0, enabled2 = (flags & 8) != 0;
  int l1 = p->window1left == 0 ? -extra : p->window1left;
  int r1 = p->window1right == 255 ? 255 + extra : p->window1right;
  int l2 = p->window2left == 0 ? -extra : p->window2left;
  int r2 = p->window2right == 255 ? 255 + extra : p->window2right;
  bool a = (x >= l1 && x <= r1) != ((flags & 1) != 0);
  bool b = (x >= l2 && x <= r2) != ((flags & 4) != 0);
  if (!enabled1) return enabled2 && b;
  if (!enabled2) return a;
  switch ((p->wbgobjlog >> (layer * 2)) & 3) {
  case 0: return a || b;
  case 1: return a && b;
  case 2: return a != b;
  default: return a == b;
  }
}

static int read_i16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }

/* Retail streams the Mode 7 tilemap and sizes what it streams for the stock
 * 256-pixel viewport. $03:9243 keeps exactly one 1024-by-1024-unit world
 * square uploaded, anchored at $00A8/$00AA ($00:97C3 camera minus 512 plus the
 * $0A:ED00 look-ahead, slew-clamped by $03:92AA). The tilemap is 128 by 128
 * tiles - 1024 by 1024 pixels - so that square fills it exactly and the map
 * aliases the 8192-by-4096-unit world every 1024 units: a sample outside the
 * square reads the tiles another part of the course left in the same cell.
 * A widened viewport reaches outside it, which is the reported pop-in.
 *
 * $03:939E and $03:9417 build their uploads from course tables in WRAM bank
 * $7F, which the frame snapshot already carries, so the compositor can resolve
 * the same tile for any world position instead. Guest state is never written,
 * and a sample inside the square still reads the live tilemap.
 *
 * $0020/$0022 hold the same anchor but are reused as scratch afterwards and do
 * not survive every frame; $00A8/$00AA ($03:9254, $03:925E) do. */
typedef struct FzeroCourse {
  const uint8_t *bank;  /* WRAM bank $7F. */
  unsigned grid;        /* $00B0/$00B1: the block grid, placed by $00:9F4C. */
  int anchor_x, anchor_y;
  double camera_x, camera_y;
  bool valid;
} FzeroCourse;

/* Shortest-path blend of a coordinate that repeats every `period` units, the
 * rule FzeroMode7Interpolate already applies to a scanline's origin. */
static double periodic_blend(double from, double to, double alpha, double period) {
  return from + alpha * remainder(to - from, period);
}

/* The Mode 7 centre is the camera reduced to the map, but retail writes either
 * representative: on some frames it is the camera's map position and on others
 * that plus 1024. Both describe the same place and a frame's own origin and
 * centre always agree, so a texel minus its own centre is exact - but an
 * interpolated origin takes the shortest path across the seam and can land in
 * the other representative. Subtracting the wrong one moves every Mode 7
 * sample a whole map period and repaints the screen from another part of the
 * course for that one presentation. Blend the centre and the camera the same
 * periodic way so they stay in the origin's representative. */

static FzeroCourse course_open(const FzeroSourceFrame *f, bool world) {
  FzeroCourse course = {NULL, 0, 0, 0, 0, 0, false};
  if (!world) return course;
  course.bank = f->ram + 0x10000;
  course.grid = (unsigned)f->ram[0xb0] | ((unsigned)f->ram[0xb1] << 8);
  /* $03:9346 and $03:9381 select the streamed strip with ($14 & $03F0) and
   * ($12 & $03F0), so the square starts on a 16-unit block boundary whatever
   * the anchor's low bits are. Align down, or the last block row and column
   * are classified outside and resolved twice over. */
  course.anchor_x = read_i16(f->ram + 0xa8) & 0x1ff0;
  course.anchor_y = read_i16(f->ram + 0xaa) & 0x0ff0;
  course.camera_x = read_i16(f->ram + 0xb70);
  course.camera_y = read_i16(f->ram + 0xb90);
  course.valid = true;
  return course;
}

static int course_centre(const int16_t matrix[8], int index) {
  return ((int)(matrix[index] & 0x1fff) ^ 0x1000) - 0x1000;
}

static unsigned course_word(const FzeroCourse *course, unsigned address) {
  return (unsigned)course->bank[address & 0xffff] |
         ((unsigned)course->bank[(address + 1) & 0xffff] << 8);
}

/* Three indirections, all in bank $7F, following $03:93BF..$03:93E7 (and
 * $03:9438..$03:9463, which resolves the same tile for the column strip):
 * ($B0),Y selects a block from a 32-by-16 grid of 256-unit cells; the block id
 * times 32 picks one of sixteen 16-unit sub-rows in the $5000 pointer table;
 * that sub-row lists sixteen pointers to 2-by-2 tile groups. Each group's four
 * bytes go to $4A00/$4A80 at X and X+1, so they read (x0,y0) (x0,y1) (x1,y0)
 * (x1,y1). Every world position resolves - the grid spans the whole 8192-by-
 * 4096-unit world - so there is no void case to handle. */
static unsigned course_tile(const FzeroCourse *course, int world_x, int world_y) {
  unsigned block = course->bank[(course->grid + ((world_y >> 8) & 15) * 32 +
                                 ((world_x >> 8) & 31)) & 0xffff];
  unsigned row = course_word(course, 0x5000 + block * 32 + ((world_y >> 4) & 15) * 2);
  unsigned group = course_word(course, row + ((world_x >> 4) & 15) * 2);
  return course->bank[(group + ((world_x >> 3) & 1) * 2 +
                       ((world_y >> 3) & 1)) & 0xffff];
}

/* Tile number for one Mode 7 texel, or -1 to keep the live tilemap. The
 * transform is centred on the camera, so the texel's offset from the 13-bit
 * centre is exact even where the wrapped coordinate is ambiguous. */
/* Neighbouring samples on a scanline share an eight-unit cell - hundreds of
 * them in the near field, where a pixel advances a third of a unit - and the
 * tile depends on nothing finer, so one entry retires the three dependent
 * loads for every sample after the first in each cell. */
typedef struct FzeroCourseCache { int cell_x, cell_y, tile; } FzeroCourseCache;

static const FzeroCourseCache kCourseCacheEmpty = {-1, -1, -1};

/* What one scanline measures its texels against. The per-line blend keeps the
 * texel and the centre in the same frame and the camera anchors them to the
 * world; a texel is already whole, so the whole conversion collapses to one
 * integer offset per scanline instead of two floors per sample. */
typedef struct FzeroCourseLine { int offset_x, offset_y; } FzeroCourseLine;

static FzeroCourseLine course_line(double camera_x, double camera_y,
                                   double centre_x, double centre_y) {
  return (FzeroCourseLine){(int)floor(camera_x - centre_x),
                           (int)floor(camera_y - centre_y)};
}

static int course_sample(const FzeroCourse *course, const FzeroCourseLine *line,
                         FzeroCourseCache *cache, FzeroMode7Texel texel) {
  /* Written so a NaN fails the comparison rather than reaching the cast. */
  if (!course->valid || !(fabs(texel.x) < 1e6 && fabs(texel.y) < 1e6)) return -1;
  int world_x = ((int)texel.x + line->offset_x) & 0x1fff;
  int world_y = ((int)texel.y + line->offset_y) & 0x0fff;
  if (((world_x - course->anchor_x) & 0x1fff) < 1024 &&
      ((world_y - course->anchor_y) & 0x0fff) < 1024) return -1;
  int cell_x = world_x >> 3, cell_y = world_y >> 3;
  if (cell_x != cache->cell_x || cell_y != cache->cell_y) {
    cache->cell_x = cell_x;
    cache->cell_y = cell_y;
    cache->tile = (int)course_tile(course, world_x, world_y);
  }
  return cache->tile;
}

/* $0081DE DMA-orders six 32-byte vehicle reservations using $0AC0..$0ACA.
 * Resolve the reservation, not screen proximity: nearby cars may overlap or
 * swap drawing order. $F468 records the used opponent tiles at $11D0+2*car. */
static int object_owner(const FzeroSourceFrame *f, int slot) {
  if (!f->ram[0x50]) return -1;
  int car = -1;
  if (slot >= 68 && slot < 116) {
    int source = read_i16(f->ram + 0xac0 + ((slot - 68) / 8) * 2);
    if (source >= 0x300 && source <= 0x3a0 && !(source & 31))
      car = (source - 0x300) / 32;
  } else if (slot >= 116) {
    /* $C339 alternates odd/even opponent shadows. NMI increments $51
     * after the source was built, so this snapshot contains the next parity. */
    car = 1 + ((f->ram[0x51] ^ 1) & 1) + ((slot - 116) / 4) * 2;
  }
  if (car < 0 || car >= 6 || (f->ram[0xb00 + car * 2] & 0x88) != 0x88) return -1;
  return car;
}

static int object_x(const FzeroSourceFrame *f, int raw_x, FzeroViewport viewport, int owner) {
  if (owner >= 0 && viewport.enhanced) {
    int cx = read_i16(f->ram + 0xc50 + owner * 2);
    return raw_x + 512 * (int)round((cx - raw_x) / 512.0);
  }
  return raw_x >= 256 ? raw_x - 512 : raw_x;
}

static void sprites(const Ppu *p, const FzeroSourceFrame *frame,
                    const FzeroSourceFrame *previous, double alpha,
                    int y, FzeroViewport viewport, bool race_hud, bool results,
                    int owner_filter, uint16_t *pixels) {
  const FzeroRasterLine *line = &frame->lines[y];
  const uint16_t *vram = frame->vram;
  static const int sizes[8][2] = {{8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}};
  memset(pixels, 0, (size_t)viewport.width * sizeof(*pixels));
  for (int slot = 127; slot >= 0; --slot) {
    unsigned position = line->oam[slot * 2], attr = line->oam[slot * 2 + 1];
    unsigned high = line->high_oam[slot / 4] >> ((slot % 4) * 2);
    int size = sizes[p->obsel >> 5][(high >> 1) & 1];
    int sprite_y = position >> 8;
    int raw_x = (position & 255) | ((high & 1) << 8);
    if (raw_x == 384 && sprite_y == 128) continue;
    /* $B164 draws the intro's spare-machine icon/count in temporary slots
     * 126/127 ($03F8/$03FC); setup later transfers them to HUD slots 22/23.
     * They already belong to the right edge while the course name is centered. */
    bool intro_counter = !race_hud && frame->ram[0x58] == 0 &&
        (results || frame->ram[0x55] <= 2) && slot >= 126;
    int owner = intro_counter ? -1 : object_owner(frame, slot);
    if (owner_filter >= 0 && owner != owner_filter) continue;
    /* The screen-locked player and unowned effects use offscreen X as a
     * hiding mechanism, sometimes retaining Y and stale tile attributes.
     * Only verified opponent reservations can reveal those signed positions. */
    if (viewport.enhanced && raw_x >= 256 && owner <= 0) continue;
    if (viewport.enhanced && owner == 0 && attr == 0) continue;
    if (viewport.enhanced && slot >= 68 && slot < 116 && owner > 0 &&
        (slot - 68) % 8 >= frame->ram[0x11d0 + owner * 2]) continue;
    int x = object_x(frame, raw_x, viewport, owner);
    /* Match a live car and the same tile reservation before interpolating.
     * HUD values, births/deaths, reused slots and sprite animation changes
     * remain discrete. Guest positions are never written by this pass. */
    if (previous && owner >= 0 && alpha < 1 &&
        !memcmp(previous->ram + 0xb00 + owner * 2, frame->ram + 0xb00 + owner * 2, 2)) {
      const FzeroRasterLine *old = &previous->lines[y];
      int old_slot = slot;
      if (slot >= 68 && slot < 116) {
        old_slot = -1;
        for (int block = 68; block < 116; block += 8)
          if (object_owner(previous, block) == owner) { old_slot = block + (slot - 68) % 8; break; }
      }
      if (old_slot >= 0) {
      unsigned old_position = old->oam[old_slot * 2];
      unsigned old_high = old->high_oam[old_slot / 4] >> ((old_slot % 4) * 2);
      int old_y = old_position >> 8, old_owner = object_owner(previous, old_slot);
      int old_x = object_x(previous, (old_position & 255) | ((old_high & 1) << 8), viewport, old_owner);
      if (old->oam[old_slot * 2 + 1] == attr && old_owner == owner &&
          ((old_high ^ high) & 2) == 0 && abs(old_x - x) <= 32 && abs(old_y - sprite_y) <= 24) {
        x = (int)round(old_x + alpha * (x - old_x));
        sprite_y = (int)round(old_y + alpha * (sprite_y - old_y));
      }
      }
    }
    int row = (y - sprite_y) & 255;
    if (row >= size) continue;
    /* Map/markers 20..31, timer/boosts 32..46, rank 48..51. Slot 47 is
     * NOT HUD: $00:BED7..BF5E writes the player's skid/collision spark at
     * $02BC using the vehicle's $0C70/$0C80 position. Moving it to the right
     * edge detaches it from the car whenever the effect appears (#4).
     * $EDB3/$EE93 reuse 48..63 for explosion/smoke pieces. Only anchor
     * 48..51 when they contain the rank digits ($180..$189/$190..$199,
     * written by $A8B1), not merely because they occupy rank's slots. */
    unsigned tile_number = attr & 0x1ff;
    bool rank_digit = slot >= 48 && slot < 52 &&
        (tile_number & 0x1e0) == 0x180 && (tile_number & 15) <= 9;
    if (race_hud && ((slot >= 20 && slot < 47) || rank_digit)) {
      if (x < 0 || x >= 256) continue;
      x += (slot < 22 || (slot >= 24 && slot < 32) || slot >= 48) ?
          -viewport.extra : viewport.extra;
    }
    if (intro_counter) x += viewport.extra;
    x += viewport.extra;
    if (attr & 0x8000) row = size - 1 - row;
    unsigned base = (p->obsel & 7) << 13;
    if (attr & 0x100) base += (((p->obsel & 0x18) + 8) << 9);
    unsigned palette = 128 + ((attr >> 9) & 7) * 16;
    unsigned priority = ((attr >> 12) & 3) * 4 + 2;
    unsigned layer = attr & 0x800 ? 4 : 6; /* OBJ palettes 0..3 bypass math. */
    for (int col = 0; col < size; ++col) {
      int dest = x + col;
      if (dest < 0 || dest >= viewport.width) continue;
      int cx = attr & 0x4000 ? size - 1 - col : col;
      unsigned tile = ((((attr & 255) >> 4) + row / 8) << 4) |
                       (((attr & 15) + cx / 8) & 15);
      unsigned pixel = tile_pixel(vram, base + tile * 16, cx & 7, row & 7, 4);
      if (pixel) pixels[dest] = (priority << 12) | (layer << 8) | palette | pixel;
    }
  }
}

static bool window_condition(unsigned mode, bool inside) {
  return mode == 3 || (mode == 1 && !inside) || (mode == 2 && inside);
}

static uint32_t colour(const Ppu *p, const uint16_t *palette, uint16_t main,
                       uint16_t sub, bool inside) {
  unsigned rgb = palette[main & 255], layer = (main >> 8) & 15;
  bool clipped = window_condition(p->cgwsel >> 6, inside);
  bool math = !window_condition((p->cgwsel >> 4) & 3, inside) &&
              ((p->cgadsub & 63) & (1u << layer));
  unsigned other = p->fixedColor;
  bool half = math && (p->cgadsub & 64) && !clipped;
  if (math && (p->cgwsel & 2)) {
    if ((sub & 255) != 0) other = palette[sub & 255];
    else half = false;
  }
  uint32_t result = 0;
  for (int component = 0; component < 3; ++component) {
    int c = clipped ? 0 : (rgb >> (component * 5)) & 31;
    if (math) {
      int second = (other >> (component * 5)) & 31;
      c += p->cgadsub & 128 ? -second : second;
      if (c < 0) c = 0;
      if (half) c /= 2;
      if (c > 31) c = 31;
    }
    c = ((c << 3) | (c >> 2)) * (p->inidisp & 15) / 15;
    result |= (uint32_t)c << (16 - component * 8);
  }
  return result;
}

static FzeroMode7Line hd_transform(const FzeroSourceFrame *frame, int y) {
  const uint8_t *registers = frame->lines[y].registers;
  int16_t matrix[8];
  memcpy(matrix, registers + offsetof(Ppu, m7matrix), sizeof(matrix));
  SnesMode7HdTransform t = SnesMode7HdMakeTransform(
      matrix, registers[offsetof(Ppu, m7sel)], (unsigned)y + 1);
  return (FzeroMode7Line){t.origin_x * 256, t.origin_y * 256,
                         t.step_x * 256, t.step_y * 256, t.control};
}

static FzeroMode7Line hd_frame_transform(const FzeroSourceFrame *frame,
                                         const FzeroSourceFrame *previous,
                                         int y, double alpha, bool interpolate) {
  FzeroMode7Line t = hd_transform(frame, y);
  if (interpolate && alpha < 1 &&
      (previous->lines[y].registers[offsetof(Ppu, bgmode)] & 7) == 7)
    t = FzeroMode7Interpolate(hd_transform(previous, y), t, alpha);
  return t;
}

static void expand_line(uint32_t *out, const uint32_t *row,
                        int y, int width, unsigned scale) {
  uint32_t *first = out + (size_t)y * scale * width * scale;
  for (int x = 0; x < width; ++x)
    for (unsigned sx = 0; sx < scale; ++sx) first[x * scale + sx] = row[x];
  for (unsigned sy = 1; sy < scale; ++sy)
    memcpy(first + (size_t)sy * width * scale, first, (size_t)width * scale * sizeof(*out));
}

typedef struct FzeroHdPixelContext {
  const uint32_t *colors;
  uint16_t objects[2];
  uint8_t flags; /* BG enabled on main/sub, then colour-window membership. */
} FzeroHdPixelContext;

static bool render_frame(uint32_t *out, FzeroViewport viewport, double alpha,
                          unsigned scale, uint32_t *native) {
  const FzeroSourceFrame *f = &frames[current];
  const FzeroSourceFrame *previous = &frames[current ^ 1];
  if (!f->valid || !out || viewport.width < 256 || viewport.width > FZERO_MAX_WIDTH ||
      viewport.width != 256 + 2 * viewport.extra) return false;
  memset(out, 0, (size_t)viewport.width * 224 * scale * scale * sizeof(*out));
  if (native) memset(native, 0, (size_t)viewport.width * 224 * sizeof(*native));
  /* $81 selects live track scenery on the title screen as well as in races.
   * Scene $54=2 additionally owns vehicle identity and adaptive race HUD. */
  bool scenery = f->ram[0x81] != 0;
  /* Scene 3 serves both the race/attract exit and the black results/menu.
   * Both can fade in phase 5. Distinguish the actual published PPU layout:
   * results disable the track backgrounds, retaining BG3 + OBJ ($94), while
   * the live race keeps BG1/BG2 ($17). Phase alone splits END GAME's reused
   * OBJ slots 20..30 across the viewport as soon as its fade begins. */
  memcpy(&scanout, f->lines[100].registers, PPU_SAVESTATE_REGS_SIZE);
  bool results = scenery && f->ram[0x54] == 3 &&
      !(scanout.screenEnabled[0] & 3);
  bool race_exit = f->ram[0x54] == 3 &&
      !results && (f->ram[0x55] == 4 || f->ram[0x55] == 5);
  bool world = scenery && (f->ram[0x54] == 2 || race_exit);
  /* Phase 6 is still the live YOU LOST view in both GP and Training; its
   * timer, power fill and spare machines all retain the race HUD layout. */
  /* $8ACD installs the race HUD before $8B11 advances setup substate $56.
   * Setup phase $55=2 then displays it while waiting to enter active phase 3.
   * Anchor tiles, sprites and the power meter as soon as that HUD is ready;
   * the preceding course-title/setup phase still uses centered reservations. */
  bool race_hud = world && (f->ram[0x55] >= 3 ||
                            (f->ram[0x55] == 2 && f->ram[0x56] != 0));
  bool interpolate = world && previous->valid && previous->frame + 1 == f->frame &&
      !memcmp(previous->ram + 0x54, f->ram + 0x54, 3) &&
      previous->ram[0x81] == f->ram[0x81];
  if (interpolate) {
    int angle_change = abs((int)previous->ram[0xac] - f->ram[0xac]);
    if (angle_change > 96) angle_change = 192 - angle_change;
    if (angle_change > 16 ||
        abs((int)remainder(read_i16(previous->ram + 0xb70) - read_i16(f->ram + 0xb70), 8192)) > 128 ||
        abs((int)remainder(read_i16(previous->ram + 0xb90) - read_i16(f->ram + 0xb90), 4096)) > 128)
      interpolate = false;
  }
  /* Only a widened viewport reaches outside retail's streamed square, and
   * only a live race scene has course tables to resolve it from. */
  FzeroCourse course = course_open(f, world && viewport.enhanced);
  uint16_t object_pixels[FZERO_MAX_WIDTH];
  uint32_t row[FZERO_MAX_WIDTH];
  for (int y = 0; y < 224; ++y) {
    const FzeroRasterLine *l = &f->lines[y];
    memcpy(&scanout, l->registers, PPU_SAVESTATE_REGS_SIZE);
    if (scanout.inidisp & 128) continue;
    int mode = scanout.bgmode & 7;
    if (!scenery || (mode != 1 && mode != 7)) {
      /* Flat selection/loading screens keep their original centered artwork,
       * but their backdrop, fades and colour windows cover the full viewport. */
      for (int sx = 0; sx < viewport.width; ++sx)
        row[sx] = colour(&scanout, l->palette, 0x500, 0x500,
            in_window(&scanout, 5, sx - viewport.extra, viewport.extra));
      memcpy(row + viewport.extra, f->stock + y * 256, 256 * sizeof(*out));
      expand_line(out, row, y, viewport.width, scale);
      if (native) memcpy(native + y * viewport.width, row, viewport.width * sizeof(*native));
      continue;
    }
    FzeroMode7Line transform = FzeroMode7Transform(scanout.m7matrix, scanout.m7sel, y + 1);
    double camera_x = course.camera_x, camera_y = course.camera_y;
    double centre_x = course_centre(scanout.m7matrix, 4);
    double centre_y = course_centre(scanout.m7matrix, 5);
    FzeroCourseCache cache = kCourseCacheEmpty;
    if (mode == 7 && interpolate && alpha < 1) {
      Ppu old;
      memcpy(&old, previous->lines[y].registers, PPU_SAVESTATE_REGS_SIZE);
      if ((old.bgmode & 7) == 7) {
        FzeroMode7Line before = FzeroMode7Transform(old.m7matrix, old.m7sel, y + 1);
        /* FzeroMode7Interpolate keeps the current scanline whenever it cannot
         * blend, so the centre and the camera must follow the same decision:
         * measuring a blended texel against an unblended centre, or the other
         * way round, moves every sample a whole map period. */
        double blend = FzeroMode7Blend(&before, &transform, alpha);
        transform = FzeroMode7Interpolate(before, transform, alpha);
        if (blend < 1) {
          centre_x = periodic_blend(course_centre(old.m7matrix, 4), centre_x, blend, 1024);
          centre_y = periodic_blend(course_centre(old.m7matrix, 5), centre_y, blend, 1024);
          camera_x = periodic_blend(read_i16(previous->ram + 0xb70), camera_x, blend, 8192);
          camera_y = periodic_blend(read_i16(previous->ram + 0xb90), camera_y, blend, 4096);
        }
      }
    }
    FzeroCourseLine reference = course_line(camera_x, camera_y, centre_x, centre_y);
    if (world || results)
      sprites(&scanout, f, interpolate ? previous : NULL, alpha, y, viewport,
              race_hud, results, -1, object_pixels);
    else
      memset(object_pixels, 0, (size_t)viewport.width * sizeof(*object_pixels));
    bool hd_line = scale > 1 && world && mode == 7 &&
        !((scanout.mosaic & 1) && (scanout.mosaic >> 4)) &&
        !(scanout.setini & 0x49) && !(scanout.cgwsel & 1);
    if (!hd_line || native) {
      for (int sx = 0; sx < viewport.width; ++sx) {
        int x = sx - viewport.extra;
        uint16_t screens[2] = {0x500, 0x500};
        for (int sub = 0; sub < 2; ++sub) {
          for (int layer = 0; layer < (mode == 7 ? 1 : 3); ++layer) {
            if (!(scanout.screenEnabled[sub] & (1u << layer))) continue;
            if ((scanout.screenWindowed[sub] & (1u << layer)) && in_window(&scanout, layer, x, viewport.extra)) continue;
            uint16_t pixel;
            if (mode == 7) {
              FzeroMode7Texel texel = FzeroMode7Locate(&transform, x);
              unsigned index = FzeroMode7Fetch(&transform, f->vram, texel,
                  course_sample(&course, &reference, &cache, texel));
              pixel = index ? 0x5000 | index : 0;
            } else {
              int bx = x;
              if (layer == 2 && results && y < 32 && sx < 128) {
                bx = sx; /* Top-left score; lap table/menu stays centered. */
              } else if (layer == 2 && !race_hud) {
                if (x < 0 || x >= 256 || (results && y < 32 && x < 128)) continue;
              }
              if (layer == 2 && race_hud) {
                bx = sx < viewport.width / 2 ? sx : sx - 2 * viewport.extra;
                if ((sx < viewport.width / 2 && bx >= 128) ||
                    (sx >= viewport.width / 2 && bx < 128)) continue;
              }
              pixel = background_pixel(&scanout, f->vram, layer, bx, y + 1,
                                       viewport.enhanced && (x < 0 || x >= 256));
            }
            if (pixel > screens[sub]) screens[sub] = pixel;
          }
          if ((scanout.screenEnabled[sub] & 16) &&
              (!(scanout.screenWindowed[sub] & 16) || !in_window(&scanout, 4, x, viewport.extra)) &&
              object_pixels[sx] > screens[sub]) screens[sub] = object_pixels[sx];
        }
        /* The power meter is filled by the colour window, not a BG tile.
         * Its HDMA band must travel with the right-anchored BG3 outline. */
        /* Loss keeps its score at the left edge, but its collapsed colour
         * window must not expand into the former race meter/HDMA panel. */
        int colour_x = results ? sx : x;
        if (race_hud && mode == 1 && y >= 19 && y <= 27 &&
            scanout.window1left >= 176 && scanout.window1right <= 239)
          colour_x -= viewport.extra;
        row[sx] = colour(&scanout, l->palette, screens[0], screens[1],
            in_window(&scanout, 5, colour_x, results ? 0 : viewport.extra));
      }
      /* Preserve the meter's composed fill, including its fixed-colour HDMA,
       * without letting a different section of skyline show through it. */
      if (race_hud && viewport.enhanced && mode == 1 && y >= 19 && y <= 27)
        memcpy(row + 176 + 2 * viewport.extra,
               f->stock + y * 256 + 176, 64 * sizeof(*out));
      /* Title/menu graphics remain an exact centered group. Only their live
       * scenery expands; hidden/reused OBJ reservations cannot leak into it. */
      if (!world && !results)
        memcpy(row + viewport.extra,
               f->stock + y * 256, 256 * sizeof(*out));
      if (native) memcpy(native + y * viewport.width, row, viewport.width * sizeof(*native));
      if (!hd_line) expand_line(out, row, y, viewport.width, scale);
    }
    if (!hd_line) continue;

    /* Window membership and sprite priority are native-pixel properties.
     * Resolve them once for all subpixels. In columns without sprites, the
     * colour math depends only on the sampled palette index: cache that
     * mapping per scanline/window combination, including transparent zero.
     * Sprite columns retain the full main/subscreen colour calculation. */
    FzeroHdPixelContext contexts[FZERO_MAX_WIDTH];
    uint32_t colors[8][256];
    unsigned colors_ready = 0;
    for (int sx = 0; sx < viewport.width; ++sx) {
      int x = sx - viewport.extra;
      FzeroHdPixelContext *c = &contexts[sx];
      c->flags = in_window(&scanout, 5, x, viewport.extra) ? 4 : 0;
      for (int sub = 0; sub < 2; ++sub) {
        if ((scanout.screenEnabled[sub] & 1) &&
            (!(scanout.screenWindowed[sub] & 1) ||
             !in_window(&scanout, 0, x, viewport.extra))) c->flags |= 1u << sub;
        c->objects[sub] = (scanout.screenEnabled[sub] & 16) &&
            (!(scanout.screenWindowed[sub] & 16) ||
             !in_window(&scanout, 4, x, viewport.extra)) ? object_pixels[sx] : 0;
      }
      c->colors = NULL;
      if (!(c->objects[0] | c->objects[1])) {
        unsigned key = c->flags;
        if (!(colors_ready & (1u << key))) {
          for (unsigned index = 0; index < 256; ++index)
            colors[key][index] = colour(&scanout, l->palette,
                (key & 1) && index ? (uint16_t)(0x5000 | index) : 0x500,
                (key & 2) && index ? (uint16_t)(0x5000 | index) : 0x500,
                (key & 4) != 0);
          colors_ready |= 1u << key;
        }
        c->colors = colors[key];
      }
    }
    FzeroMode7Line hd = hd_frame_transform(f, previous, y, alpha, interpolate);
    FzeroMode7Line next = hd;
    bool adjacent = false;
    if (y + 1 < 224) {
      const uint8_t *next_registers = f->lines[y + 1].registers;
      /* Smooth only the same camera's contiguous Mode 7 band. HUD, IRQ
       * splits, flips, fades and changes of coordinate origin are boundaries. */
      adjacent = (next_registers[offsetof(Ppu, bgmode)] & 7) == 7 &&
          next_registers[offsetof(Ppu, m7sel)] == scanout.m7sel &&
          next_registers[offsetof(Ppu, inidisp)] == scanout.inidisp &&
          next_registers[offsetof(Ppu, setini)] == scanout.setini &&
          next_registers[offsetof(Ppu, mosaic)] == scanout.mosaic &&
          !memcmp(next_registers + offsetof(Ppu, m7matrix) + 8, scanout.m7matrix + 4, 8);
      if (adjacent) next = hd_frame_transform(f, previous, y + 1, alpha, interpolate);
    }
    SnesMode7HdTransform affine = SnesMode7HdMakeTransform(
        scanout.m7matrix, scanout.m7sel, (unsigned)y + 1);
    for (unsigned sy = 0; sy < scale; ++sy) {
      double fraction = (double)sy / scale;
      FzeroMode7Line subline = hd;
      if (adjacent) subline = FzeroMode7Interpolate(hd, next, fraction);
      else {
        subline.origin_x += affine.row_x * fraction * 256;
        subline.origin_y += affine.row_y * fraction * 256;
      }
      uint32_t *destination = out + ((size_t)y * scale + sy) * viewport.width * scale;
      FzeroMode7Texel last = {NAN, NAN};
      unsigned index = 0;
      for (int sx = 0; sx < viewport.width; ++sx) {
        int x = sx - viewport.extra;
        const FzeroHdPixelContext *c = &contexts[sx];
        for (unsigned sample = 0; sample < scale; ++sample) {
          FzeroMode7Texel texel = FzeroMode7Locate(&subline, x + (double)sample / scale);
          /* Near-camera HD samples often hit the same source texel. The
           * immutable texture/course lookup is independent of screen masks
           * and sprites, so reuse its index across those column boundaries. */
          if (texel.x != last.x || texel.y != last.y) {
            index = FzeroMode7Fetch(&subline, f->vram, texel,
                course_sample(&course, &reference, &cache, texel));
            last = texel;
          }
          if (c->colors) {
            destination[sx * scale + sample] = c->colors[index];
          } else {
            uint16_t screens[2] = {0x500, 0x500};
            for (int sub = 0; sub < 2; ++sub) {
              if ((c->flags & (1u << sub)) && index) screens[sub] = (uint16_t)(0x5000 | index);
              if (c->objects[sub] > screens[sub]) screens[sub] = c->objects[sub];
            }
            destination[sx * scale + sample] = colour(&scanout, l->palette,
                screens[0], screens[1], (c->flags & 4) != 0);
          }
        }
      }
    }
  }
  return true;
}

bool FzeroRendererDraw(uint32_t *out, FzeroViewport viewport, double alpha) {
  return render_frame(out, viewport, alpha, 1, NULL);
}

bool FzeroRendererDrawHd(uint32_t *out, size_t capacity,
                         FzeroViewport viewport, double alpha, unsigned scale) {
  return FzeroRendererDrawPresentation(NULL, out, capacity, viewport, alpha, scale);
}

bool FzeroRendererDrawPresentation(uint32_t *native, uint32_t *out, size_t capacity,
                                   FzeroViewport viewport, double alpha, unsigned scale) {
  if (!FzeroValidHdScale(scale) || viewport.width < 256 ||
      viewport.width > FZERO_MAX_WIDTH ||
      capacity < (size_t)viewport.width * 224 * scale * scale) return false;
  return render_frame(out, viewport, alpha, scale, native);
}

static void sky_cache_trace(const char *reason, int index) {
  if (!getenv("FZERO_TRIPLE_SKY_TRACE")) return;
  static unsigned hit_count, miss_count;
  if (!strcmp(reason, "hit")) {
    if (++hit_count <= 5 || hit_count % 120 == 0)
      fprintf(stderr, "[fzero-triple-sky] hit=%u miss=%u\n", hit_count, miss_count);
  } else if (++miss_count <= 10 || miss_count % 120 == 0) {
    fprintf(stderr, "[fzero-triple-sky] miss=%u hit=%u reason=%s index=%d\n",
            miss_count, hit_count, reason, index);
  }
}

static bool cached_atlas_matches(const FzeroSourceFrame *f, int rows) {
  if (!sky_atlas.valid || sky_atlas.rows != rows) {
    sky_cache_trace("shape", rows); return false;
  }
  /* Only these registers affect BG tile pixel fetches. Scroll is applied
   * when sampling the atlas; brightness, windows, colour math and the current
   * palette are applied afterwards from the current captured scanline. */
  const size_t mode = offsetof(Ppu, bgmode);
  const size_t maps = offsetof(Ppu, bgXsc);
  const size_t tiles = offsetof(Ppu, bgTileAdr);
  for (int y = 0; y < rows; ++y) {
    const FzeroRasterLine *line = &f->lines[y];
    int offset = -1;
    if (line->registers[mode] != sky_atlas.registers[y][mode]) offset = (int)mode;
    else if (memcmp(line->registers + maps,
                    sky_atlas.registers[y] + maps, 2)) offset = (int)maps;
    else if (memcmp(line->registers + tiles,
                    sky_atlas.registers[y] + tiles, sizeof(uint16_t))) offset = (int)tiles;
    if (offset >= 0) {
      sky_cache_trace("register", y * PPU_SAVESTATE_REGS_SIZE + offset);
      return false;
    }
  }
  for (int a = 0; a < 0x8000; ++a)
    if (sky_atlas.vram_used[a] && f->vram[a] != sky_atlas.vram[a]) {
      sky_cache_trace("vram", a); return false;
    }
  sky_cache_trace("hit", 0);
  return true;
}

static bool sky_atlas_supported(const FzeroSourceFrame *f, int rows) {
  if (rows > 51) return false; /* background_pixel's panorama strip range. */
  Ppu *p = &scanout; /* PPU_bigTiles expects a pointer identifier. */
  for (int y = 0; y < rows; ++y) {
    memcpy(&scanout, f->lines[y].registers, PPU_SAVESTATE_REGS_SIZE);
    for (int layer = 0; layer < 2; ++layer) {
      int first = layer == 0 ? 36 : 92;
      int scroll = scanout.vScroll[layer];
      if (PPU_bigTiles(p, layer) ||
          scanout.bgXsc[layer] != (layer == 0 ? 0x79 : 0x71) ||
          scanout.hScroll[layer] >= 256 || scroll < first || scroll > 204 ||
          (scroll - first) % 56) return false;
    }
  }
  return true;
}

static bool build_sky_atlas(const FzeroSourceFrame *f, int rows) {
  static const int periods[2] = {896, 768};
  for (int layer = 0; layer < 2; ++layer) {
    size_t needed = (size_t)rows * periods[layer];
    if (sky_atlas.capacity[layer] < needed) {
      uint16_t *pixels = realloc(sky_atlas.pixels[layer],
                                 needed * sizeof(*pixels));
      if (!pixels) return false;
      sky_atlas.pixels[layer] = pixels;
      sky_atlas.capacity[layer] = needed;
    }
  }
  sky_atlas.valid = false;
  memset(sky_atlas.vram_used, 0, sizeof(sky_atlas.vram_used));
  sky_vram_reads = sky_atlas.vram_used;
  for (int y = 0; y < rows; ++y) {
    memcpy(&scanout, f->lines[y].registers, PPU_SAVESTATE_REGS_SIZE);
    for (int layer = 0; layer < 2; ++layer) {
      uint16_t h = scanout.hScroll[layer], v = scanout.vScroll[layer];
      scanout.hScroll[layer] = 0;
      scanout.vScroll[layer] = layer == 0 ? 36 : 92;
      for (int x = 0; x < periods[layer]; ++x)
        sky_atlas.pixels[layer][(size_t)y * periods[layer] + x] =
            background_pixel(&scanout, f->vram, layer, x, y + 1, true);
      scanout.hScroll[layer] = h;
      scanout.vScroll[layer] = v;
    }
    memcpy(sky_atlas.registers[y], f->lines[y].registers,
           PPU_SAVESTATE_REGS_SIZE);
  }
  sky_vram_reads = NULL;
  memcpy(sky_atlas.vram, f->vram, sizeof(sky_atlas.vram));
  sky_atlas.rows = rows;
  sky_atlas.valid = true;
  return true;
}

static bool draw_triple_sides(uint32_t *output, size_t capacity,
                              const FzeroTripleRig *rig, int logical_width,
                              bool direct_sky) {
  static FzeroTripleVec3 *cached_edge_rays, *cached_ground_rays;
  static FzeroTripleRig cached_rig;
  if (!output || !rig || rig->panel_width_px < 2 || rig->panel_height_px < 2 ||
      rig->panel_width_px > 4096 || rig->panel_height_px > 2160 ||
      capacity < (size_t)2 * rig->panel_width_px * rig->panel_height_px)
    return false;
  const FzeroSourceFrame *f = &frames[current];
  /* Countdown substate 2 already has the live Mode 7 track and calibrated
   * camera. Accept it so the first racing presentation does not briefly
   * clear both side panels while the captured WRAM trails the guest state. */
  if (!f->valid || f->ram[0x54] != 2 || f->ram[0x55] < 2 || !f->ram[0x81])
    return false;
  /* A high-refresh presenter may show the same emulated frame more than once.
   * The two ground panels are expensive per-pixel projections and do not use
   * presentation alpha; retain them until the next published source frame. */
  if (triple_cache.valid && triple_cache.output == output &&
      triple_cache.logical_width == logical_width &&
      triple_cache.direct_sky == direct_sky &&
      !memcmp(&triple_cache.rig, rig, sizeof(*rig))) return true;
  FzeroTripleSurface panels[3];
  if (!FzeroTripleBuild(rig, panels)) return false;
  const int pw = rig->panel_width_px, ph = rig->panel_height_px;
  /* The normal rational ground path only needs rays at the two vertical
   * edges for skyline angle and the optional old-horizon comparison. */
  bool use_row_projection = !getenv("FZERO_TRIPLE_DISABLE_ROW");
  memcpy(&scanout, f->lines[80].registers, PPU_SAVESTATE_REGS_SIZE);
  if ((scanout.bgmode & 7) != 7) return false;
  FzeroMode7Line far = FzeroMode7Transform(scanout.m7matrix, scanout.m7sel, 81);
  memcpy(&scanout, f->lines[180].registers, PPU_SAVESTATE_REGS_SIZE);
  if ((scanout.bgmode & 7) != 7) return false;
  FzeroMode7Line near = FzeroMode7Transform(scanout.m7matrix, scanout.m7sel, 181);
  FzeroTripleGround ground;
  if (!FzeroTripleGroundCalibrate(rig, logical_width, far, 80, near, 180, &ground))
    return false;
  if (!cached_edge_rays || memcmp(&cached_rig, rig, sizeof(*rig))) {
    FzeroTripleVec3 *rays = malloc((size_t)4 * pw * sizeof(*rays));
    if (!rays) return false;
    for (int side = 0; side < 2; ++side)
      for (int edge = 0; edge < 2; ++edge)
        for (int x = 0; x < pw; ++x)
          if (!FzeroTripleRay(&panels[side ? 2 : 0], x,
              edge ? ph - 1 : 0, pw, ph,
              &rays[((size_t)side * 2 + edge) * pw + x])) {
            free(rays);
            return false;
          }
    free(cached_edge_rays);
    free(cached_ground_rays);
    cached_edge_rays = rays;
    cached_ground_rays = NULL;
    cached_rig = *rig;
  }
  if (!use_row_projection && !cached_ground_rays) {
    FzeroTripleVec3 *rays = malloc((size_t)2 * pw * ph * sizeof(*rays));
    if (!rays) return false;
    for (int side = 0; side < 2; ++side)
      for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
          if (!FzeroTripleRay(&panels[side ? 2 : 0], x, y, pw, ph,
              &rays[(size_t)side * pw * ph + (size_t)y * pw + x])) {
            free(rays);
            return false;
          }
    cached_ground_rays = rays;
  }

  FzeroCourse course = course_open(f, true);
  /* The race switches to BG mode 1 for the skyline, then Mode 7 for the
   * track. Map a panel's horizontal eye ray onto the existing panoramic BG1/
   * BG2 strips. Calibrate the angular scale at the center-panel edges so the
   * side seams meet the same columns as the normal widened compositor; the
   * physical bezel gap then naturally skips a small wedge of panorama.
   * Horizontal ray angle does not vary with pixel Y, so compute it once. */
  int sky_columns[2 * 4096];
  double half_angle = atan(rig->width_mm / (2.0 * rig->eye_distance_mm));
  if (!(half_angle > 0.0) || !isfinite(half_angle)) return false;
  double pixels_per_radian = (logical_width * 0.5) / half_angle;
  for (int side = 0; side < 2; ++side)
    for (int x = 0; x < pw; ++x) {
      FzeroTripleVec3 ray = cached_edge_rays[(size_t)side * 2 * pw + x];
      double column = 128.0 + atan2(ray.x, -ray.z) * pixels_per_radian;
      if (!isfinite(column) || fabs(column) > 100000.0) return false;
      sky_columns[side * pw + x] = (int)lround(column);
    }
  /* Mode 1 occupies the skyline band above the Mode 7 IRQ split. Its stock
   * rows alone leave a triangular black gap on the turned panels, because a
   * physical side ray can remain above the ground horizon after that split.
   * Sample each BG1/BG2 panorama row by panel angle, then lower its skyline to
   * the ground-plane horizon per column. The topmost blue sky fills any area
   * outside the guest panorama's vertical range. BG3/OBJ/HUD remain center-only. */
  int sky_count = 0;
  while (sky_count < 80) {
    memcpy(&scanout, f->lines[sky_count].registers, PPU_SAVESTATE_REGS_SIZE);
    if ((scanout.bgmode & 7) != 1 || (scanout.inidisp & 128)) break;
    ++sky_count;
  }
  bool use_atlas = sky_count && !direct_sky &&
                   !getenv("FZERO_TRIPLE_DISABLE_ATLAS") &&
                   sky_atlas_supported(f, sky_count);
  if (use_atlas && !cached_atlas_matches(f, sky_count) &&
      !build_sky_atlas(f, sky_count)) return false;
  uint32_t *sky_pixels = sky_count ?
      malloc((size_t)sky_count * 2 * pw * sizeof(uint32_t)) : NULL;
  if (sky_count && !sky_pixels) return false;
  static const int periods[2] = {896, 768};
  for (int source_y = 0; source_y < sky_count; ++source_y) {
    const FzeroRasterLine *raster = &f->lines[source_y];
    memcpy(&scanout, raster->registers, PPU_SAVESTATE_REGS_SIZE);
    for (int side = 0; side < 2; ++side)
      for (int x = 0; x < pw; ++x) {
        int column = sky_columns[side * pw + x];
        uint16_t screens[2] = {0x500, 0x500};
        for (int sub = 0; sub < 2; ++sub)
          for (int layer = 0; layer < 2; ++layer) {
            if (!(scanout.screenEnabled[sub] & (1u << layer)) ||
                ((scanout.screenWindowed[sub] & (1u << layer)) &&
                 in_window(&scanout, layer, column, 0))) continue;
            uint16_t pixel;
            if (use_atlas) {
              int first = layer == 0 ? 36 : 92;
              int panorama_x = ((scanout.vScroll[layer] - first) / 56 * 256 +
                                scanout.hScroll[layer] + column) % periods[layer];
              if (panorama_x < 0) panorama_x += periods[layer];
              pixel = sky_atlas.pixels[layer][(size_t)source_y * periods[layer] + panorama_x];
            } else {
              pixel = background_pixel(&scanout, f->vram, layer,
                                       column, source_y + 1, true);
            }
            if (pixel > screens[sub]) screens[sub] = pixel;
          }
        sky_pixels[(size_t)source_y * 2 * pw + side * pw + x] =
            colour(&scanout, raster->palette, screens[0], screens[1], false);
      }
  }
  if (sky_count) {
    int sky_shift[2 * 4096];
    bool exact_horizon = !getenv("FZERO_TRIPLE_DISABLE_EXACT_HORIZON");
    for (int side = 0; side < 2; ++side)
      for (int x = 0; x < pw; ++x) {
        double horizon;
        if (exact_horizon) {
          if (!FzeroTripleGroundHorizon(&ground, &panels[side ? 2 : 0],
                                       x, pw, ph, &horizon)) {
            free(sky_pixels); return false;
          }
        } else {
          const FzeroTripleVec3 top = cached_edge_rays[(size_t)side * 2 * pw + x];
          const FzeroTripleVec3 bottom =
              cached_edge_rays[((size_t)side * 2 + 1) * pw + x];
          double d0 = -top.z * ground.pitch_sin - top.y * ground.pitch_cos;
          double d1 = -bottom.z * ground.pitch_sin - bottom.y * ground.pitch_cos;
          horizon = fabs(d1 - d0) > 1e-9 ? -d0 * (ph - 1) / (d1 - d0) :
                    d0 > 0 ? 0.0 : (double)ph;
        }
        if (!isfinite(horizon)) { free(sky_pixels); return false; }
        sky_shift[side * pw + x] = (int)lround(sky_count - 1 - horizon * 224.0 / ph);
      }
    for (int y = 0; y < ph; ++y) {
      int native_y = (int)((y + 0.5) * 224 / ph);
      for (int side = 0; side < 2; ++side)
        for (int x = 0; x < pw; ++x) {
          int sky_y = native_y + sky_shift[side * pw + x];
          if (sky_y < 0) sky_y = 0;
          if (sky_y >= sky_count) sky_y = sky_count - 1;
          output[(size_t)side * pw * ph + (size_t)y * pw + x] =
              sky_pixels[(size_t)sky_y * 2 * pw + side * pw + x];
        }
    }
    free(sky_pixels);
  } else {
    memset(output, 0, (size_t)2 * pw * ph * sizeof(*output));
  }
  /* Flat-ground projection is rational in panel X. The direct ray path stays
   * available for pixel-exact offline A/B checks of new camera fixtures. */
  for (int y = 0; y < ph; ++y) {
    int source_y = (int)((y + 0.5) * 224 / ph);
    if (source_y > 223) source_y = 223;
    const FzeroRasterLine *raster = &f->lines[source_y];
    memcpy(&scanout, raster->registers, PPU_SAVESTATE_REGS_SIZE);
    if (scanout.inidisp & 128) continue;
    if ((scanout.bgmode & 7) != 7) continue;
    FzeroMode7Line line = FzeroMode7Transform(scanout.m7matrix, scanout.m7sel,
                                               (unsigned)source_y + 1);
    FzeroCourseLine reference = course_line(course.camera_x, course.camera_y,
        course_centre(scanout.m7matrix, 4), course_centre(scanout.m7matrix, 5));
    FzeroTripleVec3 left_ray, right_ray;
    FzeroMode7Texel align_left, align_right;
    bool align = FzeroTripleRay(&panels[1], pw / 2 - 1, y, pw, ph, &left_ray) &&
        FzeroTripleRay(&panels[1], pw / 2, y, pw, ph, &right_ray) &&
        FzeroTripleGroundLocate(&ground, left_ray, &align_left) &&
        FzeroTripleGroundLocate(&ground, right_ray, &align_right);
    FzeroTripleLineAlignment alignment;
    align = align && FzeroTripleGroundBuildLineAlignment(line, align_left,
        align_right, logical_width, pw, &alignment);
    for (int side = 0; side < 2; ++side) {
      FzeroTripleGroundRow row;
      if (use_row_projection &&
          !FzeroTripleGroundBuildRow(&ground, &panels[side ? 2 : 0],
                                     y, pw, ph, &row)) return false;
      FzeroCourseCache cache = kCourseCacheEmpty;
      for (int x = 0; x < pw; ++x) {
        FzeroMode7Texel texel;
        bool located = false;
        if (align && use_row_projection)
          located = FzeroTripleGroundRowLocate(&row, x, &texel);
        else if (align) {
          FzeroTripleVec3 ray = cached_ground_rays[(size_t)side * pw * ph +
              (size_t)y * pw + x];
          located = FzeroTripleGroundLocate(&ground, ray, &texel);
        }
        if (!located ||
            !FzeroTripleGroundApplyLineAlignment(&alignment, texel,
                &texel)) continue;
        texel.x = floor(texel.x);
        texel.y = floor(texel.y);
        int tile = course_sample(&course, &reference, &cache, texel);
        unsigned index = FzeroMode7Fetch(&line, f->vram, texel, tile);
        uint16_t layer = index ? (uint16_t)(0x5000 | index) : 0x500;
        uint16_t main = scanout.screenEnabled[0] & 1 ? layer : 0x500;
        uint16_t sub = scanout.screenEnabled[1] & 1 ? layer : 0x500;
        output[(size_t)side * pw * ph + (size_t)y * pw + x] =
            colour(&scanout, raster->palette, main, sub, false);
      }
    }
  }
  triple_cache.output = output;
  triple_cache.rig = *rig;
  triple_cache.logical_width = logical_width;
  triple_cache.direct_sky = direct_sky;
  if (++triple_cache.version == 0) ++triple_cache.version;
  triple_cache.valid = true;
  return true;
}

bool FzeroRendererDrawTripleSides(uint32_t *output, size_t capacity,
                                  const FzeroTripleRig *rig, int logical_width) {
  return draw_triple_sides(output, capacity, rig, logical_width, false);
}

bool FzeroRendererDrawTripleSidesDirectSky(uint32_t *output, size_t capacity,
                                           const FzeroTripleRig *rig,
                                           int logical_width) {
  return draw_triple_sides(output, capacity, rig, logical_width, true);
}

uint64_t FzeroRendererTripleSidesVersion(void) { return triple_cache.version; }

bool FzeroRendererProbeTripleVehicles(const FzeroTripleRig *rig,
                                      int logical_width,
                                      FzeroTripleVehicleProbe out[6]) {
  const FzeroSourceFrame *f = &frames[current];
  if (!rig || !out || logical_width < 256 ||
      logical_width > FZERO_MAX_WIDTH || !f->valid || f->ram[0x54] != 2 ||
      f->ram[0x55] < 3 || !f->ram[0x81]) return false;
  FzeroTripleSurface panels[3];
  if (!FzeroTripleBuild(rig, panels)) return false;
  memcpy(&scanout, f->lines[80].registers, PPU_SAVESTATE_REGS_SIZE);
  if ((scanout.bgmode & 7) != 7) return false;
  FzeroMode7Line far = FzeroMode7Transform(scanout.m7matrix, scanout.m7sel, 81);
  memcpy(&scanout, f->lines[180].registers, PPU_SAVESTATE_REGS_SIZE);
  if ((scanout.bgmode & 7) != 7) return false;
  FzeroMode7Line near = FzeroMode7Transform(scanout.m7matrix, scanout.m7sel, 181);
  FzeroTripleGround ground;
  if (!FzeroTripleGroundCalibrate(rig, logical_width, far, 80, near, 180,
                                 &ground)) return false;
  FzeroMode7Texel center = {course_centre(scanout.m7matrix, 4),
                            course_centre(scanout.m7matrix, 5)};
  int camera_x = read_i16(f->ram + 0xb70);
  int camera_y = read_i16(f->ram + 0xb90);
  const FzeroRasterLine *raster = &f->lines[100];
  memset(out, 0, 6 * sizeof(*out));
  for (int car = 0; car < 6; ++car) {
    FzeroTripleVehicleProbe *probe = &out[car];
    probe->state = f->ram[0xb00 + car * 2];
    probe->world_x = read_i16(f->ram + 0xb70 + car * 2);
    probe->world_y = read_i16(f->ram + 0xb90 + car * 2);
    probe->guest_x = read_i16(f->ram + 0xc50 + car * 2);
    probe->guest_y = read_i16(f->ram + 0xc60 + car * 2);
    probe->raster_left = probe->raster_top = -1;
    probe->raster_right = probe->raster_bottom = -1;
    if (!probe->state) continue;
    for (int slot = 68; slot < 128; ++slot) {
      if (object_owner(f, slot) != car) continue;
      unsigned position = raster->oam[slot * 2];
      unsigned high = raster->high_oam[slot / 4] >> ((slot % 4) * 2);
      unsigned raw_x = (position & 255) | ((high & 1) << 8);
      if (raw_x == 384 && (position >> 8) == 128) continue;
      if (!raster->oam[slot * 2 + 1]) continue;
      ++probe->oam_slots;
    }
    FzeroMode7Texel texel;
    if (!FzeroTripleGroundWorldTexel(probe->world_x, probe->world_y,
                                     camera_x, camera_y, center, &texel))
      continue;
    for (int side = 0; side < 3; ++side)
      probe->projected[side] = FzeroTripleGroundProject(&ground, &panels[side],
          texel, rig->panel_width_px, rig->panel_height_px,
          &probe->panel_x[side], &probe->panel_y[side]);
    bool on_side = false;
    for (int side = 0; side < 3; side += 2)
      on_side |= probe->projected[side] &&
          probe->panel_x[side] >= 0 &&
          probe->panel_x[side] < rig->panel_width_px &&
          probe->panel_y[side] >= 0 &&
          probe->panel_y[side] < rig->panel_height_px;
    if (car <= 0 || !on_side || !probe->oam_slots) continue;
    FzeroViewport viewport = {FZERO_MAX_WIDTH,
                              (FZERO_MAX_WIDTH - 256) / 2, 0, true};
    uint16_t sprite_row[FZERO_MAX_WIDTH];
    for (int y = 0; y < 224; ++y) {
      memcpy(&scanout, f->lines[y].registers, PPU_SAVESTATE_REGS_SIZE);
      if ((scanout.inidisp & 128) || (scanout.bgmode & 7) != 7) continue;
      sprites(&scanout, f, NULL, 1.0, y, viewport, true, false,
              car, sprite_row);
      for (int x = 0; x < FZERO_MAX_WIDTH; ++x) {
        if (!sprite_row[x]) continue;
        ++probe->raster_sprite_pixels;
        int logical_x = x - viewport.extra;
        if (probe->raster_sprite_pixels == 1) {
          probe->raster_left = probe->raster_right = logical_x;
          probe->raster_top = probe->raster_bottom = y;
        } else {
          if (logical_x < probe->raster_left) probe->raster_left = logical_x;
          if (logical_x > probe->raster_right) probe->raster_right = logical_x;
          if (y < probe->raster_top) probe->raster_top = y;
          if (y > probe->raster_bottom) probe->raster_bottom = y;
        }
      }
    }
    if (!probe->raster_sprite_pixels) continue;
    double dx = texel.x - ground.camera_x, dy = texel.y - ground.camera_y;
    double lateral = dx * ground.right_x + dy * ground.right_y;
    double forward = (dx * ground.forward_x + dy * ground.forward_y) /
                     ground.forward_scale;
    FzeroTripleVec3 anchor = {lateral,
        -ground.camera_height * ground.pitch_cos + forward * ground.pitch_sin,
        -ground.camera_height * ground.pitch_sin - forward * ground.pitch_cos};
    if (!isfinite(anchor.x) || !isfinite(anchor.y) ||
        !isfinite(anchor.z) || !(anchor.z < -1.0)) continue;
    double scale_x = rig->width_mm / logical_width *
                     (-anchor.z / rig->eye_distance_mm);
    double scale_y = rig->height_mm / 224.0 *
                     (-anchor.z / rig->eye_distance_mm);
    if (!isfinite(scale_x) || !isfinite(scale_y) ||
        !(scale_x > 0 && scale_y > 0)) continue;
    probe->billboard_valid = true;
    probe->billboard_x = anchor.x;
    probe->billboard_y = anchor.y;
    probe->billboard_z = anchor.z;
    probe->billboard_scale_x = scale_x;
    probe->billboard_scale_y = scale_y;
    for (int panel = 0; panel < 3; ++panel) {
      double left = INFINITY, top = INFINITY, right = -INFINITY, bottom = -INFINITY;
      unsigned corners_projected = 0;
      for (int corner = 0; corner < 4; ++corner) {
        int sx = corner & 1 ? probe->raster_right + 1 : probe->raster_left;
        int sy = corner & 2 ? probe->raster_bottom + 1 : probe->raster_top;
        FzeroTripleVec3 point = {anchor.x + (sx - probe->guest_x) * scale_x,
                                 anchor.y - (sy - probe->guest_y) * scale_y,
                                 anchor.z};
        double px, py;
        if (!FzeroTripleProjectDirection(&panels[panel], point,
                                         rig->panel_width_px,
                                         rig->panel_height_px,
                                         &px, &py)) continue;
        ++corners_projected;
        if (px < left) left = px;
        if (px > right) right = px;
        if (py < top) top = py;
        if (py > bottom) bottom = py;
      }
      if (corners_projected != 4 || !isfinite(left) || !isfinite(top)) continue;
      probe->billboard_projected[panel] = true;
      probe->billboard_left[panel] = left;
      probe->billboard_top[panel] = top;
      probe->billboard_right[panel] = right;
      probe->billboard_bottom[panel] = bottom;
    }
  }
  return true;
}

bool FzeroRendererPreviewTripleVehicles(uint32_t *sides, size_t capacity,
                                        const FzeroTripleRig *rig,
                                        int logical_width,
                                        unsigned *written_pixels) {
  if (written_pixels) *written_pixels = 0;
  if (!sides || !rig || !written_pixels || logical_width < 256 ||
      logical_width > FZERO_MAX_WIDTH || rig->panel_width_px < 2 ||
      rig->panel_height_px < 2 ||
      rig->panel_width_px > 4096 || rig->panel_height_px > 2160 ||
      capacity < (size_t)2 * rig->panel_width_px * rig->panel_height_px)
    return false;
  FzeroTripleVehicleProbe probes[6];
  if (!FzeroRendererProbeTripleVehicles(rig, logical_width, probes)) return false;
  FzeroTripleSurface panels[3];
  if (!FzeroTripleBuild(rig, panels)) return false;
  const FzeroSourceFrame *f = &frames[current];
  const int pw = rig->panel_width_px, ph = rig->panel_height_px;
  const size_t area = (size_t)pw * ph;
  const FzeroViewport viewport = {FZERO_MAX_WIDTH,
                                  (FZERO_MAX_WIDTH - 256) / 2, 0, true};
  const size_t source_count = (size_t)FZERO_MAX_WIDTH * 224;
  uint16_t *raster = malloc(source_count * sizeof(*raster));
  uint32_t *colors = malloc(source_count * sizeof(*colors));
  if (!raster || !colors) { free(raster); free(colors); return false; }
  for (int car = 1; car < 6; ++car) {
    const FzeroTripleVehicleProbe *probe = &probes[car];
    if ((probe->state & 0x88) != 0x88 || !probe->oam_slots ||
        !probe->raster_sprite_pixels || !probe->billboard_valid) continue;
    FzeroTripleVec3 anchor = {probe->billboard_x, probe->billboard_y,
                              probe->billboard_z};
    double scale_x = probe->billboard_scale_x;
    double scale_y = probe->billboard_scale_y;
    for (int y = probe->raster_top; y <= probe->raster_bottom; ++y) {
      if (y < 0 || y >= 224) continue;
      memcpy(&scanout, f->lines[y].registers, PPU_SAVESTATE_REGS_SIZE);
      sprites(&scanout, f, NULL, 1.0, y, viewport, true, false, car,
              raster + (size_t)y * FZERO_MAX_WIDTH);
      for (int x = probe->raster_left; x <= probe->raster_right; ++x) {
        int sx = x + viewport.extra;
        if (sx < 0 || sx >= FZERO_MAX_WIDTH) continue;
        size_t index = (size_t)y * FZERO_MAX_WIDTH + sx;
        if (raster[index] > 0x5000)
          colors[index] = colour(&scanout, f->lines[y].palette,
                                 raster[index], 0x500, false);
      }
    }
    for (int side = 0; side < 3; side += 2) {
      if (!probe->billboard_projected[side] ||
          probe->billboard_right[side] < 0 ||
          probe->billboard_left[side] >= pw ||
          probe->billboard_bottom[side] < 0 ||
          probe->billboard_top[side] >= ph) continue;
      int x0 = (int)fmax(0, floor(probe->billboard_left[side]) - 2);
      int x1 = (int)fmin(pw - 1, ceil(probe->billboard_right[side]) + 2);
      int y0 = (int)fmax(0, floor(probe->billboard_top[side]) - 2);
      int y1 = (int)fmin(ph - 1, ceil(probe->billboard_bottom[side]) + 2);
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
          FzeroTripleVec3 ray;
          if (!FzeroTripleRay(&panels[side], x, y, pw, ph, &ray) ||
              fabs(ray.z) < 1e-9) continue;
          double distance = anchor.z / ray.z;
          if (!isfinite(distance) || !(distance > 0)) continue;
          double source_x = probe->guest_x +
                            (distance * ray.x - anchor.x) / scale_x;
          double source_y = probe->guest_y -
                            (distance * ray.y - anchor.y) / scale_y;
          if (!isfinite(source_x) || !isfinite(source_y) ||
              source_x < probe->raster_left - 1.0 ||
              source_x > probe->raster_right + 1.0 ||
              source_y < probe->raster_top - 1.0 ||
              source_y > probe->raster_bottom + 1.0) continue;
          int sx = (int)lround(source_x);
          int sy = (int)lround(source_y);
          if (sx < probe->raster_left || sx > probe->raster_right ||
              sy < probe->raster_top || sy > probe->raster_bottom ||
              sy < 0 || sy >= 224) continue;
          sx += viewport.extra;
          if (sx < 0 || sx >= FZERO_MAX_WIDTH) continue;
          size_t source = (size_t)sy * FZERO_MAX_WIDTH + sx;
          if (raster[source] <= 0x5000) continue;
          sides[(size_t)(side ? 1 : 0) * area + (size_t)y * pw + x] = colors[source];
          ++*written_pixels;
        }
    }
  }
  free(raster);
  free(colors);
  return true;
}
