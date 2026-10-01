/* Include implementation to compare the private ground cache against the
 * unchanged generic converter, without adding production test entry points. */
#include "../src/fzero_renderer.c"
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%d: %s\n", __LINE__, #e); return 1; } } while (0)

int main(void) {
  Ppu p = {0};
  uint16_t palette[256];
  GroundColourCache cache;
  unsigned long long checked = 0;
  /* Every relevant register combination, every byte index, all brightness
   * levels and main/sub enable combinations. Palette patterns cover channel
   * extremes, mixed channels and the zero-index backdrop path. */
  for (unsigned pattern = 0; pattern < 4; ++pattern) {
    for (unsigned i = 0; i < 256; ++i)
      palette[i] = pattern == 0 ? 0 : pattern == 1 ? 32767 :
          pattern == 2 ? (uint16_t)((i & 31) | ((31-(i&31))<<5) | (((i>>3)&31)<<10)) :
          (uint16_t)((i*109+137)&32767);
    for (unsigned window = 0; window < 32; ++window)
      for (unsigned math = 0; math < 16; ++math)
        for (unsigned brightness = 0; brightness < 16; ++brightness)
          for (unsigned screens = 0; screens < 4; ++screens) {
            p.cgwsel = (window & 1)*2 | ((window>>1)&3)*16 | ((window>>3)&3)*64;
            p.cgadsub = (math&1) | ((math>>1)&1)*32 | ((math>>2)&1)*64 | ((math>>3)&1)*128;
            p.inidisp = brightness;
            p.fixedColor = (uint16_t)((window*997+math*331+brightness*73)&32767);
            p.screenEnabled[0] = screens&1;
            p.screenEnabled[1] = (screens>>1)&1;
            memset(cache.valid, 0, sizeof(cache.valid));
            for (unsigned i = 0; i < 256; ++i) {
              uint16_t layer = i ? (uint16_t)(0x5000|i) : 0x500;
              uint16_t main = p.screenEnabled[0]&1 ? layer : 0x500;
              uint16_t sub = p.screenEnabled[1]&1 ? layer : 0x500;
              uint32_t expected = colour(&p,palette,main,sub,false);
              CHECK(ground_colour(&cache,&p,palette,i)==expected);
              CHECK(cache.valid[i>>5] & (UINT32_C(1)<<(i&31)));
              CHECK(ground_colour(&cache,&p,palette,i)==expected);
              ++checked;
            }
          }
  }
  /* Exhaust all 32x32 channel pairs (integer clipping/half/brightness),
   * independently on RGB, plus invalidation after register/palette changes. */
  p.screenEnabled[0]=1; p.screenEnabled[1]=0; p.cgwsel=0;
  for (unsigned channel=0; channel<3; ++channel)
    for (unsigned first=0; first<32; ++first)
      for (unsigned second=0; second<32; ++second)
        for (unsigned math=0; math<4; ++math)
          for (unsigned brightness=0; brightness<16; ++brightness) {
            palette[1]=(uint16_t)(first<<(channel*5));
            p.fixedColor=(uint16_t)(second<<(channel*5));
            p.cgadsub=1 | (math&1)*64 | (math>>1)*128;
            p.inidisp=brightness;
            memset(cache.valid,0,sizeof(cache.valid));
            CHECK(ground_colour(&cache,&p,palette,1)==colour(&p,palette,0x5001,0x500,false));
            ++checked;
          }
  p.cgadsub=0; p.inidisp=15; palette[1]=0;
  memset(cache.valid,0,sizeof(cache.valid));
  CHECK(ground_colour(&cache,&p,palette,1)==0);
  CHECK(cache.valid[0]&2);
  palette[1]=31;
  CHECK(ground_colour(&cache,&p,palette,1)==0); /* cached black remains valid */
  memset(cache.valid,0,sizeof(cache.valid)); /* next output row */
  CHECK(ground_colour(&cache,&p,palette,1)==0xff0000);
  p.inidisp=0;
  memset(cache.valid,0,sizeof(cache.valid));
  CHECK(ground_colour(&cache,&p,palette,1)==0);
  printf("%llu exhaustive ground-colour comparisons passed\n",checked);
  return 0;
}
