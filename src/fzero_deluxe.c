#include "fzero_deluxe.h"
#include "cpu_state.h"
#include "common_rtl.h"
#include "snes/interp_bridge.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool active;
static char error[160];
bool FzeroDeluxeActive(void) { return active; }
const char *FzeroDeluxeError(void) { return error; }

bool FzeroDeluxeSelectSaveRoot(void) {
  if (!active) return true;
  /* Shared SRAM filenames are generic save.srm; a different title/prefix
   * isolates snapshots only. Give the content module its own directory too. */
  char root[96];
  if (snprintf(root, sizeof(root), "%s/bs-deluxe", RtlSaveRoot()) >= (int)sizeof(root)) {
    snprintf(error, sizeof(error), "BS Deluxe save directory is too long; choose a shorter save root.");
    return false;
  }
  RtlEnsureSaveDir();
  RtlSetSaveRoot(root);
  RtlEnsureSaveDir();
  fprintf(stderr, "[bs-deluxe] saves: %s/save.srm\n", RtlSaveRoot());
  return true;
}

#ifdef FZERO_HAS_DELUXE
extern const DispatchEntry deluxe_g_dispatch_table[];
extern const unsigned deluxe_g_dispatch_table_count;
extern const RamRoutineGuard deluxe_g_ram_routine_guards[];
extern const unsigned deluxe_g_ram_routine_guard_count;
static const uint8_t stock_hash[32] = {
  0xbf,0x16,0xc3,0xc8,0x67,0xc5,0x8e,0x2a,0xb0,0x61,0xc7,0x0d,0xe9,0x29,0x5b,0x69,
  0x30,0xd6,0x3f,0x29,0xf8,0x1c,0xc9,0x86,0xf5,0xec,0xae,0x03,0xe0,0xad,0x18,0xd2};
static const uint8_t target_hash[32] = {
  0x77,0xbb,0x37,0xbc,0xde,0xdd,0x3e,0x17,0x32,0x17,0x27,0xd5,0xed,0x6a,0x14,0x79,
  0x2a,0xa7,0xa4,0x5a,0xc0,0x4e,0x90,0x40,0xb1,0x6b,0xf5,0x64,0xbd,0x6d,0xea,0x24};
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
#endif

bool FzeroDeluxePrepare(uint8_t **rom, size_t *size, bool enabled, const char *path) {
  active = false;
  error[0] = 0;
  cpu_select_program(NULL, 0, NULL, 0);
  interp_bridge_set_scheduler_aot_policy(-1);
  if (!enabled) return true;
#ifndef FZERO_HAS_DELUXE
  (void)rom; (void)size; (void)path;
  snprintf(error, sizeof(error), "This build does not include the BS Deluxe native module.");
  return false;
#else
  uint8_t actual[32], header[80];
  if (!rom || !*rom || !size || *size != 0x80000) goto invalid;
  sha256_compute(*rom, *size, actual);
  if (memcmp(actual, stock_hash, 32)) goto invalid;
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(error, sizeof(error), "BS Deluxe data missing. Import the supplied USA 1.0 archive first.");
    return false;
  }
  uint8_t *mapped = NULL;
  if (fread(header, 1, sizeof(header), f) != sizeof(header) ||
      memcmp(header, "BSDELX1\0", 8) || u32(header+8) != 0x100000 ||
      !u32(header+12) || u32(header+12) > 0x100000 ||
      memcmp(header+16, stock_hash, 32) || memcmp(header+48, target_hash, 32)) goto failed;
  mapped = calloc(1, 0x100000);
  if (!mapped) goto failed;
  memcpy(mapped, *rom, *size);
  uint32_t end = 0;
  for (uint32_t i = 0; i < u32(header+12); ++i) {
    uint8_t row[8];
    if (fread(row, 1, 8, f) != 8) goto failed;
    uint32_t offset = u32(row), length = u32(row+4);
    if (!length || offset < end || offset >= 0x100000 || length > 0x100000-offset) goto failed;
    if (fread(mapped+offset, 1, length, f) != length) goto failed;
    end = offset+length;
  }
  if (fgetc(f) != EOF || ferror(f)) goto failed;
  sha256_compute(mapped, 0x100000, actual);
  if (memcmp(actual, target_hash, 32)) goto failed;
  fclose(f);
  free(*rom);
  *rom = mapped;
  *size = 0x100000;
  cpu_select_program(deluxe_g_dispatch_table, deluxe_g_dispatch_table_count,
                     deluxe_g_ram_routine_guards, deluxe_g_ram_routine_guard_count);
  active = true;
  /* Loading paths need interpreter parity before further AOT promotion.
   * Keep native interrupt helpers; the main scheduler uses the faithful floor.
   * No process environment override or effect on the stock module. */
  interp_bridge_set_scheduler_aot_policy(0);
  fprintf(stderr, "[bs-deluxe] USA 1.0 native module active; separate 32 KiB saves\n");
  return true;
failed:
  free(mapped);
  fclose(f);
invalid:
  snprintf(error, sizeof(error), "BS Deluxe stock/data verification failed; nothing activated.");
  return false;
#endif
}
