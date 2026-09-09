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
      FzeroVideoSettings s; FzeroVideoDefaults(&s);
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
int main(void) {
  FzeroVideoSettings s; FzeroVideoDefaults(&s); s.enhanced = true; s.aspect = FZERO_ASPECT_32_9;
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
  puts("F-Zero renderer: bounds, immutable frames, scene fallback, car identity, signed X and panorama wrap passed");
  return 0;
}
