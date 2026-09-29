#include "fzero_playthrough.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

int main(void) {
  const char *path = "fzero-playthrough-test.fzpt";
  const char *bad = "fzero-playthrough-abort-test.fzpt";
  uint8_t digest[32] = {1}, wrong_digest[32] = {2};
  uint8_t ram[16] = {0};
  char state_path[128];
  CHECK(FzeroPlaythroughStatePath(path, state_path, sizeof(state_path)));
  CHECK(strcmp(state_path, "fzero-playthrough-test.fzpt.state") == 0);
  CHECK(!FzeroPlaythroughStatePath(path, state_path, 8));

  FzeroPlaythrough record = {0};
  CHECK(FzeroPlaythroughRecordOpen(&record, bad, digest));
  CHECK(FzeroPlaythroughRecordFrame(&record, 0x0041, ram, sizeof(ram)));
  FzeroPlaythroughAbort(&record);
  FzeroPlaythrough replay = {0};
  CHECK(!FzeroPlaythroughPlaybackOpen(&replay, bad, digest));
  CHECK(remove(bad) == 0);

  CHECK(FzeroPlaythroughRecordOpen(&record, path, digest));
  CHECK(!FzeroPlaythroughRecordOpen(&replay, path, digest)); /* no overwrite */
  CHECK(FzeroPlaythroughRecordFrame(&record, 0x0041, ram, sizeof(ram)));
  ram[1] = 42;
  CHECK(FzeroPlaythroughRecordFrame(&record, 0x0001, ram, sizeof(ram)));
  CHECK(FzeroPlaythroughClose(&record));
  CHECK(!FzeroPlaythroughPlaybackOpen(&replay, path, wrong_digest));
  CHECK(FzeroPlaythroughPlaybackOpen(&replay, path, digest));
  CHECK(replay.total == 2);
  uint32_t input = 0;
  CHECK(FzeroPlaythroughNextInput(&replay, &input) && input == 0x0041);
  CHECK(!FzeroPlaythroughVerifyFrame(&replay, ram, sizeof(ram)));
  ram[1] = 0;
  CHECK(FzeroPlaythroughVerifyFrame(&replay, ram, sizeof(ram)));
  CHECK(FzeroPlaythroughNextInput(&replay, &input) && input == 0x0001);
  ram[1] = 42;
  CHECK(FzeroPlaythroughVerifyFrame(&replay, ram, sizeof(ram)));
  CHECK(!FzeroPlaythroughNextInput(&replay, &input));
  CHECK(FzeroPlaythroughClose(&replay));
  CHECK(remove(path) == 0);
  puts("F-Zero playthrough codec tests passed");
  return 0;
}
