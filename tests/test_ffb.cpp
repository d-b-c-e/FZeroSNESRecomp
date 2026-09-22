#include "fzero_ffb.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static void put16(uint8_t *p, uint16_t value) {
  p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
}

int main() {
  uint8_t ram[0x20000]{};
  FzeroFfbState state{};
  FzeroFfbOutput out{};
  ram[0x54] = 2; ram[0x55] = 3;
  put16(ram + 0x0b70, 0x1ffe); put16(ram + 0x0b90, 100);
  FzeroFfbCompute(&state, ram, sizeof(ram), 0, 40, &out);
  CHECK(out.racing && out.constant_force == 0);
  put16(ram + 0x0b70, 2); /* wrapped movement, not a huge speed spike */
  FzeroFfbCompute(&state, ram, sizeof(ram), 0x0040, 40, &out);
  CHECK(out.constant_force > 0 && out.constant_force <= 10000);
  CHECK(out.road_magnitude > 0 && out.road_magnitude <= 10000);
  ram[0xe0] = 1;
  FzeroFfbCompute(&state, ram, sizeof(ram), 0, 100, &out);
  CHECK(out.collision_pulse == 1);
  FzeroFfbCompute(&state, ram, sizeof(ram), 0, 100, &out);
  CHECK(out.collision_pulse == 0);
  ram[0x55] = 0;
  FzeroFfbCompute(&state, ram, sizeof(ram), 0x0040, 100, &out);
  CHECK(!out.racing && out.constant_force == 0 && out.road_magnitude == 0);
  FzeroFfbCompute(&state, ram, 12, 0, 40, &out);
  CHECK(!out.racing);

#ifdef _WIN32
  /* The optional runtime must remain harmless when its configured wheel is
   * absent (and also when the DLL was not staged for this test target). */
  const char *config_path = "fzero_ffb_test.ini";
  FILE *config = std::fopen(config_path, "wb");
  CHECK(config != nullptr);
  std::fputs("[ForceFeedback]\nEnabled=1\nDevice=definitely-not-a-wheel\n",
             config);
  std::fclose(config);
  FzeroFfbInit(config_path, nullptr);
  FzeroFfbShutdown();
  FzeroFfbShutdown();
  std::remove(config_path);
#endif
  std::puts("F-Zero force-feedback model tests passed");
  return 0;
}
