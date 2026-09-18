/* BS Deluxe ships inside the executable, so the game must activate it with no
 * files beside it and must never refuse to start when something is wrong. */
#include "fzero_deluxe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

extern const uint8_t fzero_deluxe_payload[];
extern const size_t fzero_deluxe_payload_size;

static uint8_t *stock_rom(size_t *size) {
  const char *path = getenv("FZERO_TEST_ROM");
  FILE *f = fopen(path ? path : "fzero.sfc", "rb");
  if (!f) return NULL;
  uint8_t *rom = malloc(0x80000);
  if (rom && fread(rom, 1, 0x80000, f) == 0x80000 && fgetc(f) == EOF) *size = 0x80000;
  else { free(rom); rom = NULL; }
  fclose(f);
  return rom;
}

int main(void) {
  /* The payload is linked in whatever the working directory holds. */
  CHECK(fzero_deluxe_payload_size > 80);
  CHECK(!memcmp(fzero_deluxe_payload, "BSDELX1", 7));

  size_t size = 0;
  uint8_t *rom = stock_rom(&size);
  if (!rom) {
    puts("F-Zero BS Deluxe: skipped, no stock ROM available");
    return 0;
  }
  /* Nothing on disk: the embedded copy is what activates. */
  CHECK(FzeroDeluxePrepare(&rom, &size, true, "no-such-file.dat"));
  CHECK(FzeroDeluxeActive() && size == 0x100000);
  free(rom);

  /* A file that exists but does not verify falls back to the embedded copy
   * rather than failing. */
  rom = stock_rom(&size);
  CHECK(rom);
  FILE *bad = fopen("test-deluxe-bad.dat", "wb");
  CHECK(bad);
  CHECK(fwrite(fzero_deluxe_payload, 1, 64, bad) == 64); /* truncated header */
  fclose(bad);
  CHECK(FzeroDeluxePrepare(&rom, &size, true, "test-deluxe-bad.dat"));
  CHECK(FzeroDeluxeActive() && size == 0x100000);
  remove("test-deluxe-bad.dat");
  free(rom);

  /* A file that does verify is honoured, so an importer run can be tried
   * without rebuilding. */
  rom = stock_rom(&size);
  CHECK(rom);
  FILE *good = fopen("test-deluxe-good.dat", "wb");
  CHECK(good);
  CHECK(fwrite(fzero_deluxe_payload, 1, fzero_deluxe_payload_size, good) ==
        fzero_deluxe_payload_size);
  fclose(good);
  CHECK(FzeroDeluxePrepare(&rom, &size, true, "test-deluxe-good.dat"));
  CHECK(FzeroDeluxeActive() && size == 0x100000);
  remove("test-deluxe-good.dat");
  free(rom);

  /* Disabled leaves the stock cartridge alone and still succeeds. */
  rom = stock_rom(&size);
  CHECK(rom);
  CHECK(FzeroDeluxePrepare(&rom, &size, false, NULL));
  CHECK(!FzeroDeluxeActive() && size == 0x80000);
  free(rom);

  /* A ROM that is not the verified stock image is refused, and refusal is a
   * return value the host turns into "starting stock", never an exit. */
  rom = stock_rom(&size);
  CHECK(rom);
  rom[0x1234] ^= 0xff;
  CHECK(!FzeroDeluxePrepare(&rom, &size, true, NULL));
  CHECK(!FzeroDeluxeActive() && size == 0x80000);
  CHECK(FzeroDeluxeError()[0]);
  free(rom);

  puts("F-Zero BS Deluxe: embedded payload, file override, fallback and refusal passed");
  return 0;
}
