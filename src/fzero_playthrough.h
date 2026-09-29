#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Game-visible SNES input and post-frame WRAM checkpoints. The companion
 * .state file holds the exact machine state before frame zero. Playback never
 * opens a controller or a force-feedback device. */
typedef struct FzeroPlaythrough {
  FILE *stream;
  uint64_t frames;
  uint64_t total;
  uint64_t expected_hash;
  int mode; /* 0 closed, 1 recording, 2 playback */
} FzeroPlaythrough;

bool FzeroPlaythroughRecordOpen(FzeroPlaythrough *p, const char *path,
                               const uint8_t rom_sha256[32]);
bool FzeroPlaythroughPlaybackOpen(FzeroPlaythrough *p, const char *path,
                                 const uint8_t rom_sha256[32]);
bool FzeroPlaythroughStatePath(const char *path, char *out, size_t capacity);
bool FzeroPlaythroughRecordFrame(FzeroPlaythrough *p, uint32_t input,
                                const uint8_t *ram, size_t size);
bool FzeroPlaythroughNextInput(FzeroPlaythrough *p, uint32_t *input);
bool FzeroPlaythroughVerifyFrame(FzeroPlaythrough *p,
                                const uint8_t *ram, size_t size);
bool FzeroPlaythroughClose(FzeroPlaythrough *p);
void FzeroPlaythroughAbort(FzeroPlaythrough *p);
