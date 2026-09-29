#include "fzero_playthrough.h"

#include <string.h>

enum { kHeaderBytes = 49, kFrameBytes = 12, kMaxFrames = 2000000 };
static const uint8_t kMagic[8] = {'F','Z','P','T','0','0','0','1'};

static bool write_u32(FILE *f, uint32_t value) {
  uint8_t b[4] = {(uint8_t)value, (uint8_t)(value >> 8),
                  (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
  return fwrite(b, 1, sizeof(b), f) == sizeof(b);
}
static bool write_u64(FILE *f, uint64_t value) {
  return write_u32(f, (uint32_t)value) && write_u32(f, (uint32_t)(value >> 32));
}
static bool read_u32(FILE *f, uint32_t *value) {
  uint8_t b[4];
  if (fread(b, 1, sizeof(b), f) != sizeof(b)) return false;
  *value = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  return true;
}
static bool read_u64(FILE *f, uint64_t *value) {
  uint32_t lo, hi;
  if (!read_u32(f, &lo) || !read_u32(f, &hi)) return false;
  *value = (uint64_t)lo | ((uint64_t)hi << 32);
  return true;
}
static uint64_t ram_hash(const uint8_t *ram, size_t size) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < size; ++i) {
    hash ^= ram[i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}
bool FzeroPlaythroughStatePath(const char *path, char *out, size_t capacity) {
  if (!path || !*path || !out || strlen(path) + sizeof(".state") > capacity)
    return false;
  strcpy(out, path);
  strcat(out, ".state");
  return true;
}
bool FzeroPlaythroughRecordOpen(FzeroPlaythrough *p, const char *path,
                               const uint8_t rom_sha256[32]) {
  if (!p || !path || !rom_sha256) return false;
  memset(p, 0, sizeof(*p));
  p->stream = fopen(path, "wbx"); /* Never destroy an existing drive. */
  if (!p->stream) return false;
  p->mode = 1;
  uint8_t reserved[9] = {0}; /* frame count 0 and incomplete marker */
  if (fwrite(kMagic, 1, sizeof(kMagic), p->stream) != sizeof(kMagic) ||
      fwrite(rom_sha256, 1, 32, p->stream) != 32 ||
      fwrite(reserved, 1, sizeof(reserved), p->stream) != sizeof(reserved)) {
    FzeroPlaythroughAbort(p);
    return false;
  }
  return true;
}
bool FzeroPlaythroughPlaybackOpen(FzeroPlaythrough *p, const char *path,
                                 const uint8_t rom_sha256[32]) {
  if (!p || !path || !rom_sha256) return false;
  memset(p, 0, sizeof(*p));
  p->stream = fopen(path, "rb");
  if (!p->stream) return false;
  uint8_t magic[8], digest[32], complete;
  uint64_t total;
  bool valid = fread(magic, 1, 8, p->stream) == 8 &&
      fread(digest, 1, 32, p->stream) == 32 &&
      read_u64(p->stream, &total) &&
      fread(&complete, 1, 1, p->stream) == 1 &&
      !memcmp(magic, kMagic, 8) && !memcmp(digest, rom_sha256, 32) &&
      complete == 1 && total > 0 && total <= kMaxFrames;
  if (valid) {
    if (fseek(p->stream, 0, SEEK_END)) valid = false;
    else valid = ftell(p->stream) == (long)(kHeaderBytes + total * kFrameBytes) &&
                 fseek(p->stream, kHeaderBytes, SEEK_SET) == 0;
  }
  if (!valid) { FzeroPlaythroughAbort(p); return false; }
  p->total = total;
  p->mode = 2;
  return true;
}
bool FzeroPlaythroughRecordFrame(FzeroPlaythrough *p, uint32_t input,
                                const uint8_t *ram, size_t size) {
  if (!p || p->mode != 1 || !p->stream || !ram || !size ||
      p->frames >= kMaxFrames ||
      !write_u32(p->stream, input) || !write_u64(p->stream, ram_hash(ram, size))) {
    if (p) FzeroPlaythroughAbort(p);
    return false;
  }
  ++p->frames;
  return true;
}
bool FzeroPlaythroughNextInput(FzeroPlaythrough *p, uint32_t *input) {
  if (!p || p->mode != 2 || !input || p->frames >= p->total ||
      !read_u32(p->stream, input) || !read_u64(p->stream, &p->expected_hash))
    return false;
  return true;
}
bool FzeroPlaythroughVerifyFrame(FzeroPlaythrough *p,
                                const uint8_t *ram, size_t size) {
  if (!p || p->mode != 2 || !ram || !size || p->frames >= p->total ||
      ram_hash(ram, size) != p->expected_hash) return false;
  ++p->frames;
  return true;
}
bool FzeroPlaythroughClose(FzeroPlaythrough *p) {
  if (!p || !p->stream) return false;
  bool ok = true;
  if (p->mode == 1) {
    /* Only a clean, nonempty close becomes a valid immutable replay. */
    ok = p->frames > 0 && fflush(p->stream) == 0 &&
         fseek(p->stream, 40, SEEK_SET) == 0 &&
         write_u64(p->stream, p->frames) && fputc(1, p->stream) != EOF &&
         fflush(p->stream) == 0;
  } else if (p->mode == 2) {
    ok = p->frames == p->total;
  }
  if (fclose(p->stream)) ok = false;
  p->stream = NULL;
  p->mode = 0;
  return ok;
}
void FzeroPlaythroughAbort(FzeroPlaythrough *p) {
  if (!p) return;
  if (p->stream) fclose(p->stream);
  p->stream = NULL;
  p->mode = 0;
}
