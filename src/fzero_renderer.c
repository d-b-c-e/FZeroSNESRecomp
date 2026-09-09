#include "fzero_renderer.h"
#include "fzero_mode7.h"

#include <math.h>
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

bool FzeroRendererLoadCapture(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  FzeroRendererReset();
  current = 0;
  bool ok = fread(&frames[0], sizeof(frames[0]), 1, f) == 1 && fgetc(f) == EOF;
  fclose(f);
  frames[0].valid = ok;
  return ok;
}
const uint32_t *FzeroRendererStockFrame(void) { return frames[current].stock; }

void FzeroRendererReset(void) {
  frames[0].valid = frames[1].valid = false;
}
void FzeroRendererBeginFrame(const uint8_t ram[0x20000], unsigned frame) {
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
  bool selected = number && strtoul(number, NULL, 10) == f->frame;
  if (numbers) for (const char *p = numbers; *p;) {
    char *end;
    if (strtoul(p, &end, 10) == f->frame) selected = true;
    if (end == p || *end != ',') break;
    p = end + 1;
  }
  if (!selected || !prefix) return;
  char path[1024];
  char numbered_prefix[960];
  if (numbers) {
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
                    int y, FzeroViewport viewport, bool race_hud, uint16_t *pixels) {
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
    bool intro_counter = !race_hud && frame->ram[0x55] <= 2 && slot >= 126;
    int owner = intro_counter ? -1 : object_owner(frame, slot);
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
    /* Verified fixed race HUD reservations: map/markers 20..31, timer,
     * boosts and rank 32..51. Hidden HUD positions remain hidden. */
    if (race_hud && slot >= 20 && slot < 52) {
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

bool FzeroRendererDraw(uint32_t *out, FzeroViewport viewport, double alpha) {
  const FzeroSourceFrame *f = &frames[current];
  const FzeroSourceFrame *previous = &frames[current ^ 1];
  if (!f->valid || !out || viewport.width < 256 || viewport.width > FZERO_MAX_WIDTH ||
      viewport.width != 256 + 2 * viewport.extra) return false;
  memset(out, 0, (size_t)viewport.width * 224 * sizeof(*out));
  /* $81 selects live track scenery on the title screen as well as in races.
   * Scene $54=2 additionally owns vehicle identity and adaptive race HUD. */
  bool scenery = f->ram[0x81] != 0;
  bool world = scenery && f->ram[0x54] == 2;
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
  uint16_t object_pixels[FZERO_MAX_WIDTH];
  for (int y = 0; y < 224; ++y) {
    const FzeroRasterLine *l = &f->lines[y];
    memcpy(&scanout, l->registers, PPU_SAVESTATE_REGS_SIZE);
    if (scanout.inidisp & 128) continue;
    int mode = scanout.bgmode & 7;
    if (!scenery || (mode != 1 && mode != 7)) {
      /* Flat selection/loading screens keep their original centered artwork,
       * but their backdrop, fades and colour windows cover the full viewport. */
      for (int sx = 0; sx < viewport.width; ++sx)
        out[y * viewport.width + sx] = colour(&scanout, l->palette, 0x500, 0x500,
            in_window(&scanout, 5, sx - viewport.extra, viewport.extra));
      memcpy(out + y * viewport.width + viewport.extra, f->stock + y * 256, 256 * sizeof(*out));
      continue;
    }
    FzeroMode7Line transform = FzeroMode7Transform(scanout.m7matrix, scanout.m7sel, y + 1);
    if (mode == 7 && interpolate && alpha < 1) {
      Ppu old;
      memcpy(&old, previous->lines[y].registers, PPU_SAVESTATE_REGS_SIZE);
      if ((old.bgmode & 7) == 7)
        transform = FzeroMode7Interpolate(FzeroMode7Transform(old.m7matrix, old.m7sel, y + 1), transform, alpha);
    }
    if (world)
      sprites(&scanout, f, interpolate ? previous : NULL, alpha, y, viewport, race_hud, object_pixels);
    else
      memset(object_pixels, 0, (size_t)viewport.width * sizeof(*object_pixels));
    for (int sx = 0; sx < viewport.width; ++sx) {
      int x = sx - viewport.extra;
      uint16_t screens[2] = {0x500, 0x500};
      for (int sub = 0; sub < 2; ++sub) {
        for (int layer = 0; layer < (mode == 7 ? 1 : 3); ++layer) {
          if (!(scanout.screenEnabled[sub] & (1u << layer))) continue;
          if ((scanout.screenWindowed[sub] & (1u << layer)) && in_window(&scanout, layer, x, viewport.extra)) continue;
          uint16_t pixel;
          if (mode == 7) {
            unsigned index = FzeroMode7Sample(&transform, f->vram, x);
            pixel = index ? 0x5000 | index : 0;
          } else {
            int bx = x;
            if (layer == 2 && !race_hud && (x < 0 || x >= 256)) continue;
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
      int colour_x = x;
      if (race_hud && mode == 1 && y >= 19 && y <= 27 &&
          scanout.window1left >= 176 && scanout.window1right <= 239)
        colour_x -= viewport.extra;
      out[y * viewport.width + sx] = colour(&scanout, l->palette, screens[0], screens[1],
                                           in_window(&scanout, 5, colour_x, viewport.extra));
    }
    /* Preserve the meter's composed fill, including its fixed-colour HDMA,
     * without letting a different section of skyline show through it. */
    if (race_hud && viewport.enhanced && mode == 1 && y >= 19 && y <= 27)
      memcpy(out + y * viewport.width + 176 + 2 * viewport.extra,
             f->stock + y * 256 + 176, 64 * sizeof(*out));
    /* Title/menu graphics remain an exact centered group. Only their live
     * scenery expands; hidden/reused OBJ reservations cannot leak into it. */
    if (!world)
      memcpy(out + y * viewport.width + viewport.extra,
             f->stock + y * 256, 256 * sizeof(*out));
  }
  return true;
}
