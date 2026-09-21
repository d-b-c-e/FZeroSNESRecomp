#include "fzero_msu.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "sha256.h"
#include "snes/interp_bridge.h"
#include "snes/msu1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool active;
static char error[256];
/* Conn/Cubear v11, from the patch-only archive linked by its authors at
 * https://www.zeldix.net/t2768-bs-f-zero-deluxe-msu-1 . No patch is bundled. */
static const uint8_t patch_hash[32] = {
  0x90,0x19,0x01,0x3f,0x08,0x5f,0xf1,0x6f,0x55,0x01,0xc4,0x51,0x65,0x31,0xa0,0x44,
  0xbc,0x5f,0x36,0x70,0x3a,0xad,0xb5,0x88,0x44,0xe6,0x7c,0x54,0x56,0x41,0x35,0x32};
/* A non-NULL empty program prevents calls into stock/Deluxe AOT routines
 * whose baked-in instructions would bypass the imported patch. */
static const DispatchEntry patched_program[1] = {{0}};

bool FzeroMsuActive(void) { return active; }
const char *FzeroMsuError(void) { return error; }

static void set_pack(const char *value) {
#ifdef _WIN32
  _putenv_s("SNESRECOMP_MSU1", value);
#else
  setenv("SNESRECOMP_MSU1", value, 1);
#endif
}

bool FzeroMsuPrepare(uint8_t **rom, size_t *size, const char *pack, const char *rom_path) {
  active = false;
  error[0] = 0;
  if (!pack || !*pack || !strcmp(pack, "0") || !strcmp(pack, "off")) {
    set_pack("");
    return true;
  }
  char base[1024], path[1200];
  if (strlen(pack) >= sizeof(base)) goto invalid;
  snprintf(base, sizeof(base), "%s", pack);
  /* Windows batch files often include quotes in the environment VALUE. */
  size_t len = strlen(base);
  if (len >= 2 && base[0] == '"' && base[len-1] == '"') {
    memmove(base, base + 1, len - 2); base[len - 2] = 0;
  }
  if (!strcmp(base, "1") || !strcmp(base, "on") || !strcmp(base, "auto")) {
    if (snprintf(base, sizeof(base), "%s", rom_path ? rom_path : "fzero.sfc") >= (int)sizeof(base))
      goto invalid;
    char *dot = strrchr(base, '.');
    char *slash = strrchr(base, '/'), *back = strrchr(base, '\\');
    if (!slash || (back && back > slash)) slash = back;
    if (dot && (!slash || dot > slash)) *dot = 0;
  }
  const char *override = getenv("FZERO_MSU1_PATCH");
  if (override && *override) {
    if (snprintf(path, sizeof(path), "%s", override) >= (int)sizeof(path)) goto invalid;
  } else {
    struct stat st;
    char directory[1024];
    snprintf(directory, sizeof(directory), "%s", base);
    if (stat(directory, &st) != 0 || !(st.st_mode & S_IFDIR)) {
      char *slash = strrchr(directory, '/'), *back = strrchr(directory, '\\');
      if (!slash || (back && back > slash)) slash = back;
      if (slash) *slash = 0; else snprintf(directory, sizeof(directory), ".");
    }
    if (snprintf(path, sizeof(path), "%s/f-zero_msu1.ips", directory) >= (int)sizeof(path)) goto invalid;
  }
  FILE *file = fopen(path, "rb");
  if (!file) {
    snprintf(error, sizeof(error), "Put Conn/Cubear v11 f-zero_msu1.ips in the selected music folder (or set FZERO_MSU1_PATCH).");
    set_pack("");
    return false;
  }
  uint8_t patch[710], digest[32];
  size_t count = fread(patch, 1, sizeof(patch), file);
  bool read_ok = !ferror(file) && count == 709 && fgetc(file) == EOF;
  fclose(file);
  if (!read_ok) goto invalid;
  sha256_compute(patch, count, digest);
  if (memcmp(digest, patch_hash, 32)) goto invalid;
  if (!rom || !*rom || !size || (*size != 0x80000 && *size != 0x100000)) goto invalid;
  uint8_t *mapped = malloc(0x100000);
  if (!mapped) goto invalid;
  memset(mapped, 0xff, 0x100000);
  memcpy(mapped, *rom, *size);
  /* Digest-pinned IPS still gets checked bounds; never publish a partial image. */
  size_t pos = 5;
  if (memcmp(patch, "PATCH", 5)) { free(mapped); goto invalid; }
  while (pos + 3 <= count && memcmp(patch + pos, "EOF", 3)) {
    if (pos + 5 > count) { free(mapped); goto invalid; }
    size_t offset = (size_t)patch[pos] << 16 | (size_t)patch[pos+1] << 8 | patch[pos+2];
    size_t length = (size_t)patch[pos+3] << 8 | patch[pos+4];
    pos += 5;
    if (!length || pos + length > count || offset + length > 0x100000) {
      free(mapped); goto invalid;
    }
    memcpy(mapped + offset, patch + pos, length);
    pos += length;
  }
  if (pos + 3 != count || memcmp(patch + pos, "EOF", 3)) { free(mapped); goto invalid; }
  free(*rom); *rom = mapped; *size = 0x100000;
  cpu_select_program(patched_program, 0, NULL, 0);
  interp_bridge_set_scheduler_aot_policy(0);
  set_pack(base);
  active = true;
  fprintf(stderr, "[fzero-msu1] Conn/Cubear v11 active; patched cartridge runs through the interpreter\n");
  return true;
invalid:
  snprintf(error, sizeof(error), "Unsupported MSU-1 patch or path. Expected Conn/Cubear v11 (709-byte IPS); no ROM changes applied.");
  set_pack("");
  return false;
}

bool FzeroMsuSelectSaveRoot(void) {
  if (!active) return true;
  char root[96];
  if (snprintf(root, sizeof(root), "%s/msu1", RtlSaveRoot()) >= (int)sizeof(root)) {
    snprintf(error, sizeof(error), "MSU-1 save directory is too long; choose a shorter save root.");
    return false;
  }
  RtlEnsureSaveDir(); RtlSetSaveRoot(root); RtlEnsureSaveDir();
  return true;
}

void FzeroMsuRestoreAudio(const uint8_t *ram) {
  if (!active || !msu1_enabled()) return;
  msu1_write(0x2007, 0);
  /* v11 stores its selected PCM number and fallback flag in guest RAM.
   * Restart the restored song; the shared core does not serialize its cursor. */
  unsigned track = ram[0x183], command = ram[0x180];
  if (!track || track > 31 || ram[0x182] == 1) return;
  msu1_write(0x2004, (uint8_t)track); msu1_write(0x2005, 0);
  /* A snapshot between selection and the guest's ready check must restore
   * the selection too; the next guest call will start it at the right volume. */
  if (ram[0x181] == 1) { msu1_write(0x2006, 0); return; }
  msu1_write(0x2006, command == 8 ? 0 : 255);
  msu1_write(0x2007, track < 4 ? 1 : 3);
}
