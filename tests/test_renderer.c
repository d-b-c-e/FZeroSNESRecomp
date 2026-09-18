#include "fzero_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%d: %s\n", __LINE__, #e); exit(1); } } while (0)
static Ppu p;
static uint8_t ram[0x20000], before[0x20000];
static uint32_t stock[256 * 224], guarded[FZERO_MAX_WIDTH * 224 + 2];
static void word(unsigned address, unsigned value) { ram[address] = value; ram[address + 1] = value >> 8; }
static void publish(unsigned frame) {
  FzeroRendererBeginFrame(ram, frame);
  for (unsigned y = 1; y <= 224; ++y) FzeroRendererCaptureLine(&p, y);
  FzeroRendererEndFrame(&p, stock);
}
static void setup(void) {
  memset(&p, 0, sizeof(p)); memset(ram, 0, sizeof(ram));
  p.inidisp = 15; p.bgmode = 7; p.screenEnabled[0] = 1;
  p.m7matrix[0] = p.m7matrix[3] = 256;
  for (unsigned i = 0; i < 0x8000; ++i) p.vram[i] = 0x0100;
  p.cgram[1] = 31;
  for (unsigned i = 0; i < 256 * 224; ++i) stock[i] = 0x123456;
  for (unsigned i = 0; i < 128; ++i) p.oam[2*i] = 0x8080;
  memset(p.highOam, 0x55, sizeof(p.highOam));
  ram[0x54] = 2; ram[0x81] = 1;
  FzeroRendererReset();
}

static uint32_t palette_rgb(unsigned c) {
  unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
  return (((r << 3) | (r >> 2)) << 16) |
         (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
}

static void test_panorama(void) {
  /* Build an independently indexed, coloured panorama in the retail strip
   * layout. Leave padding past the stock-visible overlap empty: simply
   * widening the tilemap sampler must fail at strip/rotation boundaries. */
  for (int layer = 0; layer < 2; ++layer) {
    setup();
    memset(p.vram, 0, sizeof(p.vram));
    p.bgmode = 1; p.screenEnabled[0] = 1 << layer;
    p.bgXsc[layer] = layer == 0 ? 0x79 : 0x71;
    int base = layer == 0 ? 0x7800 : 0x7000;
    int first = layer == 0 ? 36 : 92;
    int period = layer == 0 ? 896 : 768;
    for (int i = 1; i < 128; ++i) p.cgram[i] = i;
    for (int pixel = 1; pixel <= 15; ++pixel)
      for (int y = 0; y < 8; ++y) {
        p.vram[pixel * 16 + y] = ((pixel & 1) ? 255 : 0) | ((pixel & 2) ? 0xff00 : 0);
        p.vram[pixel * 16 + y + 8] = ((pixel & 4) ? 255 : 0) | ((pixel & 8) ? 0xff00 : 0);
      }
    for (int band = 0; band * 256 < period; ++band) {
      int remaining = period - band * 256;
      int valid = (remaining < 256 ? remaining : 256) + 256;
      for (int x = 0; x < valid; x += 8) {
        int tile = ((band * 256 + x) % period) / 8;
        for (int row = 0; row < 7; ++row)
          p.vram[base + (x >= 256 ? 1024 : 0) +
                 (first / 8 + band * 7 + row) * 32 + (x / 8) % 32] =
              (tile % 15 + 1) | ((tile / 15) << 10);
      }
    }
    for (int aspect = FZERO_ASPECT_STOCK; aspect <= FZERO_ASPECT_FIT; ++aspect) {
      FzeroVideoSettings s; FzeroVideoStock(&s); /* tests build an explicit viewport, not the shipped defaults */
      s.enhanced = true; s.aspect = aspect;
      FzeroViewport v = FzeroCalculateViewport(&s, 5120, 1440);
      for (int origin = 0; origin < period; origin += 31) {
        p.hScroll[layer] = origin % 256;
        p.vScroll[layer] = first + (origin / 256) * 56;
        publish(1);
        CHECK(FzeroRendererDraw(guarded + 1, v, 1));
        for (int sx = 0; sx < v.width; ++sx) {
          int logical = (origin + sx - v.extra + period) % period;
          int tile = logical / 8;
          unsigned index = (tile / 15) * 16 + tile % 15 + 1;
          CHECK(guarded[1 + 10 * v.width + sx] == palette_rgb(p.cgram[index]));
        }
      }
    }
  }
}

static void test_hud_transition(void) {
  static uint32_t active[FZERO_MAX_WIDTH * 224];
  for (int aspect = FZERO_ASPECT_STOCK; aspect <= FZERO_ASPECT_FIT; ++aspect) {
    FzeroVideoSettings s; FzeroVideoStock(&s); /* tests build an explicit viewport, not the shipped defaults */
    s.enhanced = true; s.aspect = aspect;
    FzeroViewport v = FzeroCalculateViewport(&s, 5120, 1440);
    setup(); memset(p.vram, 0, sizeof(p.vram));
    p.bgmode = 1; p.screenEnabled[0] = 4 | 16;
    p.bgXsc[2] = 0x74; p.bgTileAdr = 0x100;
    p.vram[0x7400 + 32 + 3] = p.vram[0x7400 + 32 + 24] = 1;
    for (int y = 0; y < 8; ++y) {
      p.vram[0x1008 + y] = 255; /* red BG3 HUD tiles */
      p.vram[16 + y] = 255; /* green HUD sprites */
    }
    p.cgram[193] = 0x03e0;
    p.oam[20 * 2] = (100 << 8) | 24; p.oam[20 * 2 + 1] = 0x3801;
    p.oam[32 * 2] = (100 << 8) | 200; p.oam[32 * 2 + 1] = 0x3801;
    p.highOam[5] &= ~3; p.highOam[8] &= ~3;
    ram[0x55] = 3; publish(1);
    CHECK(FzeroRendererDraw(active, v, 1));
    CHECK(active[10 * v.width + 24] == 0xff0000);
    CHECK(active[10 * v.width + 192 + 2 * v.extra] == 0xff0000);
    CHECK(active[100 * v.width + 24] == 0x00ff00);
    CHECK(active[100 * v.width + 200 + 2 * v.extra] == 0x00ff00);

    /* Race setup already owns this HUD in substates 1 and 2, before phase 3.
     * Start from a reset too: loading a setup snapshot must not need history. */
    for (int substate = 1; substate <= 2; ++substate) {
      FzeroRendererReset();
      ram[0x55] = 2; ram[0x56] = substate; publish(2);
      CHECK(FzeroRendererDraw(guarded + 1, v, 0.5));
      CHECK(!memcmp(active, guarded + 1, v.width * 224 * sizeof(*active)));
      if (v.enhanced)
        CHECK(guarded[1 + 20 * v.width + 176 + 2 * v.extra] == 0x123456);
    }
    /* Before HUD installation, the same reservations still belong to the
     * centered intro. Do not latch the previous frame's adaptive layout. */
    ram[0x56] = 0; publish(3);
    CHECK(FzeroRendererDraw(guarded + 1, v, 1));
    CHECK(guarded[1 + 10 * v.width + v.extra + 24] == 0xff0000);
    CHECK(guarded[1 + 100 * v.width + v.extra + 24] == 0x00ff00);
    if (v.extra) CHECK(guarded[1 + 100 * v.width + 24] == 0);
    /* Attract exit still displays the race HUD while scene 3 fades it. */
    ram[0x54] = 3;
    for (int phase = 4; phase <= 5; ++phase) {
      ram[0x55] = phase; publish(4 + phase);
      CHECK(FzeroRendererDraw(guarded + 1, v, 1));
      CHECK(!memcmp(active, guarded + 1, v.width * 224 * sizeof(*active)));
    }
    /* Training loss retains the live race HUD, including OBJ timer tiles. */
    ram[0x54] = 2; ram[0x55] = 6; ram[0x58] = 1; publish(10);
    CHECK(FzeroRendererDraw(guarded + 1, v, 1));
    CHECK(!memcmp(active, guarded + 1, v.width * 224 * sizeof(*active)));
  }
}
static void test_adaptive_scenes(void) {
  for (int aspect = FZERO_ASPECT_STOCK; aspect <= FZERO_ASPECT_FIT; ++aspect) {
    FzeroVideoSettings s; FzeroVideoStock(&s); /* tests build an explicit viewport, not the shipped defaults */
    s.enhanced = true; s.aspect = aspect;
    FzeroViewport v = FzeroCalculateViewport(&s, 5120, 1440);
    setup(); ram[0x54] = 0; /* Title has live track, but no race HUD. */
    for (int scene = 0; scene < 3; ++scene) {
      if (scene == 1) { ram[0x81] = 0; p.cgram[0] = 0x03e0; }
      if (scene == 2) p.inidisp = 0; /* Backdrop fades with the scene. */
      publish(scene + 1);
      CHECK(FzeroRendererDraw(guarded + 1, v, 0.5));
      for (int y = 0; y < 224; ++y) {
        CHECK(!memcmp(guarded + 1 + y * v.width + v.extra,
                      stock + y * 256, 256 * sizeof(*stock)));
        for (int x = 0; x < v.extra; ++x) {
          uint32_t expected = scene == 0 ? 0xff0000 : scene == 1 ? 0x00ff00 : 0;
          CHECK(guarded[1 + y * v.width + x] == expected);
          CHECK(guarded[1 + y * v.width + v.width - 1 - x] == expected);
        }
      }
    }
    p.inidisp = 128; publish(4);
    CHECK(FzeroRendererDraw(guarded + 1, v, 1));
    for (int i = 0; i < v.width * 224; ++i) CHECK(guarded[1 + i] == 0);
  }
}
static void test_intro_counter(void) {
  static uint32_t intro[FZERO_MAX_WIDTH * 224];
  for (int aspect = FZERO_ASPECT_STOCK; aspect <= FZERO_ASPECT_FIT; ++aspect) {
    FzeroVideoSettings s; FzeroVideoStock(&s); /* tests build an explicit viewport, not the shipped defaults */
    s.enhanced = true; s.aspect = aspect;
    FzeroViewport v = FzeroCalculateViewport(&s, 5120, 1440);
    setup(); p.screenEnabled[0] = 16; memset(p.vram, 0, sizeof(p.vram));
    for (int y = 0; y < 8; ++y) p.vram[16 + y] = 255;
    p.cgram[193] = 0x03e0;
    p.oam[126 * 2] = (190 << 8) | 208; p.oam[126 * 2 + 1] = 0x3801;
    p.oam[127 * 2] = (198 << 8) | 232; p.oam[127 * 2 + 1] = 0x3801;
    p.highOam[31] &= ~0xf0;
    publish(1); CHECK(FzeroRendererDraw(intro, v, 0.5));
    CHECK(intro[190 * v.width + 208 + 2 * v.extra] == 0x00ff00);
    CHECK(intro[198 * v.width + 232 + 2 * v.extra] == 0x00ff00);
    /* Training reuses these slots for its course map, which stays together. */
    ram[0x58] = 1; publish(2);
    CHECK(FzeroRendererDraw(guarded + 1, v, 1));
    CHECK(guarded[1 + 190 * v.width + 208 + v.extra] == 0x00ff00);
    CHECK(guarded[1 + 198 * v.width + 232 + v.extra] == 0x00ff00);
    ram[0x58] = 0;
    ram[0x55] = 6; publish(2); /* Loss reuses the same temporary counter. */
    CHECK(FzeroRendererDraw(guarded + 1, v, 1));
    CHECK(!memcmp(intro, guarded + 1, v.width * 224 * sizeof(*intro)));
    /* Retail setup moves the same artwork into the permanent HUD slots. */
    memcpy(p.oam + 22 * 2, p.oam + 126 * 2, 4 * sizeof(*p.oam));
    p.highOam[5] &= ~0xf0;
    p.oam[126 * 2] = p.oam[127 * 2] = 0x8080; p.highOam[31] |= 0x50;
    ram[0x55] = 2; ram[0x56] = 1; publish(2);
    CHECK(FzeroRendererDraw(guarded + 1, v, 1));
    CHECK(!memcmp(intro, guarded + 1, v.width * 224 * sizeof(*intro)));
    /* All three S indicators must be right anchored on their first frame. */
    for (int i = 44; i <= 46; ++i) {
      p.oam[i * 2] = (208 << 8) | (208 + 8 * (i - 44));
      p.oam[i * 2 + 1] = 0x3801;
      p.highOam[i / 4] &= ~(3 << (2 * (i % 4)));
    }
    publish(3); CHECK(FzeroRendererDraw(intro, v, 1));
    for (int i = 0; i < 3; ++i)
      CHECK(intro[208 * v.width + 208 + 8 * i + 2 * v.extra] == 0x00ff00);
    ram[0x55] = 3; ram[0x56] = 0; publish(4);
    CHECK(FzeroRendererDraw(guarded + 1, v, 1));
    CHECK(!memcmp(intro, guarded + 1, v.width * 224 * sizeof(*intro)));
  }
}
static void test_loss_window(void) {
  setup(); ram[0x55] = 6;
  p.screenEnabled[0] = 0;
  p.windowsel = 2u << 20; /* Colour window 1: collapsed to x=0. */
  p.window1left = p.window1right = 0;
  p.cgwsel = 0x10; p.cgadsub = 0x20; p.fixedColor = 31;
  FzeroVideoSettings s; FzeroVideoStock(&s); /* tests build an explicit viewport, not the shipped defaults */
  s.enhanced = true; s.aspect = FZERO_ASPECT_21_9;
  FzeroViewport v = FzeroCalculateViewport(&s, 3440, 1440);
  publish(1); CHECK(FzeroRendererDraw(guarded + 1, v, 1));
  for (int y = 0; y < 224; ++y) {
    CHECK(guarded[1 + y * v.width] == 0xff0000);
    for (int x = 1; x < v.width; ++x) CHECK(guarded[1 + y * v.width + x] == 0);
  }
}
/* Retail streams the Mode 7 tilemap for the stock 256-pixel viewport only:
 * $03:9243 keeps one 1024x1024-unit world square uploaded, anchored at
 * $00A8/$00AA, and the tilemap aliases the 8192x4096 world every 1024 units.
 * Give the live tilemap one tile everywhere and the course tables ($00B0 block
 * grid -> $7F:5000 sub-row pointers -> 2x2 tile groups) a different one, then
 * check that only the samples outside the square follow the tables. */
static void test_course_streaming(void) {
  setup();
  ram[0x55] = 3;
  p.m7matrix[0] = p.m7matrix[3] = 256;
  p.m7matrix[1] = p.m7matrix[2] = 0;
  p.m7matrix[4] = p.m7matrix[5] = p.m7matrix[6] = p.m7matrix[7] = 0;
  p.cgram[1] = 0x001f; p.cgram[2] = 0x03e0;
  /* Tile 1 everywhere in the tilemap; character data for tiles 1 and 7. */
  for (unsigned i = 0; i < 0x4000; ++i) p.vram[i] = 1;
  for (unsigned i = 0; i < 64; ++i) {
    p.vram[1 * 64 + i] |= 1 << 8;
    p.vram[7 * 64 + i] |= 2 << 8;
  }
  /* Camera and streaming anchor at the world origin, so world x in [0,1024)
   * is inside the square and the left margin (x negative, wrapping to the top
   * of the 8192-unit world) is outside it. */
  word(0xb70, 0); word(0xb90, 0); word(0xa8, 0); word(0xaa, 0);
  uint8_t *bank = ram + 0x10000;
  ram[0xb0] = 0x00; ram[0xb1] = 0x40;          /* block grid at $7F:4000 */
  for (unsigned i = 0; i < 32 * 16; ++i) bank[0x4000 + i] = 3;
  for (unsigned i = 0; i < 16; ++i) {          /* block 3's sixteen sub-rows */
    bank[0x5000 + 3 * 32 + i * 2] = 0x00;
    bank[0x5000 + 3 * 32 + i * 2 + 1] = 0x60;  /* -> $7F:6000 */
    bank[0x6000 + i * 2] = 0x00;
    bank[0x6000 + i * 2 + 1] = 0x61;           /* -> $7F:6100 */
  }
  for (unsigned i = 0; i < 4; ++i) bank[0x6100 + i] = 7;
  publish(1);
  FzeroVideoSettings s; FzeroVideoStock(&s); /* tests build an explicit viewport, not the shipped defaults */ s.enhanced = true;
  s.aspect = FZERO_ASPECT_21_9;
  FzeroViewport v = FzeroCalculateViewport(&s, 3840, 1646);
  CHECK(v.width == 448 && v.extra == 96);
  uint32_t *out = guarded + 1;
  memcpy(before, ram, sizeof(ram));
  CHECK(FzeroRendererDraw(out, v, 1));
  CHECK(!memcmp(ram, before, sizeof(ram)));
  for (int y = 0; y < 224; ++y) {
    const uint32_t *row = out + y * v.width;
    /* Left margin: outside the square, so the course tables supply tile 7. */
    for (int x = 0; x < v.extra; ++x) CHECK(row[x] == 0x00ff00);
    /* Stock columns and the right margin stay inside it and keep tile 1. */
    for (int x = v.extra; x < v.width; ++x) CHECK(row[x] == 0xff0000);
  }
  /* Retail writes either representative of the camera's map position: some
   * frames carry the camera's own value and some that plus 1024. An
   * interpolated origin takes the shortest path across that seam, so a
   * presentation between two such frames must still resolve the same world
   * cells - subtracting the wrong representative moved every sample a whole
   * map period and repainted the screen from elsewhere on the course. */
  p.m7matrix[5] += 1024;
  word(0xb90, 0);
  publish(2);
  for (double blend = 0; blend < 1.0; blend += 0.25) {
    CHECK(FzeroRendererDraw(out, v, blend));
    for (int y = 0; y < 224; ++y) {
      const uint32_t *row = out + y * v.width;
      for (int x = 0; x < v.extra; ++x) CHECK(row[x] == 0x00ff00);
      for (int x = v.extra; x < v.width; ++x) CHECK(row[x] == 0xff0000);
    }
  }
  p.m7matrix[5] -= 1024;
  publish(3);
  CHECK(FzeroRendererDraw(out, v, 1));
  /* Stock width never consults the tables. */
  s.enhanced = false;
  v = FzeroCalculateViewport(&s, 1024, 768);
  CHECK(v.width == 256 && FzeroRendererDraw(out, v, 1));
  for (int x = 0; x < 256; ++x) CHECK(out[x] == 0xff0000);
  /* Neither does a scene that is not a live race. */
  ram[0x54] = 1; publish(2);
  s.enhanced = true; s.aspect = FZERO_ASPECT_21_9;
  v = FzeroCalculateViewport(&s, 3840, 1646);
  CHECK(FzeroRendererDraw(out, v, 1));
  CHECK(out[0] == 0xff0000 && out[v.extra] == 0x123456);
}

int main(void) {
  FzeroVideoSettings s; FzeroVideoStock(&s); /* tests build an explicit viewport, not the shipped defaults */ s.enhanced = true; s.aspect = FZERO_ASPECT_32_9;
  FzeroViewport v = FzeroCalculateViewport(&s, 5120, 1440);
  uint32_t *out = guarded + 1;
  setup();
  CHECK(!FzeroRendererDraw(out, v, 1));
  guarded[0] = guarded[v.width * 224 + 1] = 0xdeadbeef;
  publish(1); memcpy(before, ram, sizeof(ram));
  CHECK(FzeroRendererDraw(out, v, 1));
  CHECK(out[0] == 0xff0000 && out[v.width-1] == 0xff0000);
  CHECK(guarded[0] == 0xdeadbeef && guarded[v.width*224+1] == 0xdeadbeef);
  CHECK(!memcmp(ram, before, sizeof(ram)));
  memset(p.vram, 0, sizeof(p.vram));
  CHECK(FzeroRendererDraw(out, v, 1) && out[0] == 0xff0000); /* immutable publication */
  ram[0x54] = 1; publish(2);
  CHECK(FzeroRendererDraw(out, v, 0.5));
  CHECK(out[0] == 0 && out[v.extra] == 0x123456 && out[v.extra + 255] == 0x123456);
  setup(); p.screenEnabled[0] = 16; memset(p.vram, 0, sizeof(p.vram));
  for (int y = 0; y < 8; ++y) p.vram[16 + y] = 255;
  p.cgram[193] = 0x03e0;
  ram[0x50] = 1; word(0xb02, 0x88); word(0xc52, 318); word(0xc62, 80);
  word(0xac0, 0x320); ram[0x11d2] = 1;
  p.oam[68*2] = (80 << 8) | 54; p.oam[68*2+1] = 0x3801;
  publish(3);
  CHECK(FzeroRendererDraw(out, v, 1));
  CHECK(out[80*v.width + v.extra + 310] == 0x00ff00); /* 9-bit X restored on right */
  word(0xc52, (unsigned)-82); p.oam[68*2] = (80 << 8) | 166;
  publish(4);
  CHECK(FzeroRendererDraw(out, v, 1));
  CHECK(out[80*v.width + v.extra - 90] == 0x00ff00); /* left margin */
  /* Stale player/effect tiles are parked offscreen with arbitrary Y/attrs. */
  word(0xac0, 0x300); word(0xb00, 0x88);
  p.oam[68*2] = 128; p.oam[68*2+1] = 0x3801;
  p.oam[52*2] = (100 << 8) | 128; p.oam[52*2+1] = 0x3801;
  publish(5); CHECK(FzeroRendererDraw(out, v, 1));
  CHECK(out[v.extra - 128] == 0 && out[100*v.width + v.extra - 128] == 0);
  /* The intro reuses HUD reservations for centered course-title letters. */
  p.oam[20*2] = (100 << 8) | 100; p.oam[20*2+1] = 0x3801;
  p.highOam[5] &= ~3;
  ram[0x55] = 2;
  publish(6); CHECK(FzeroRendererDraw(out, v, 1));
  CHECK(out[100*v.width + v.extra + 100] == 0x00ff00);
  CHECK(out[100*v.width + 100] == 0);
  FzeroRendererReset(); CHECK(!FzeroRendererDraw(out, v, 0));
  test_panorama();
  test_hud_transition();
  test_adaptive_scenes();
  test_intro_counter();
  test_loss_window();
  test_course_streaming();
  puts("F-Zero renderer: bounds, immutable frames, scene fallback, car identity, signed X, panorama wrap and HUD transitions passed");
  return 0;
}
