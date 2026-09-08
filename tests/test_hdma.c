#include "fzero_hdma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "HDMA check failed: %s\n", #x); exit(1); } } while (0)
static uint8_t memory[65536], output[16], registers[16];
static uint32_t addresses[32];
static unsigned reads, writes;
static uint8_t read_bus(void *ctx, uint32_t address, uint8_t latch) {
  (void)ctx;
  CHECK(reads < 32); addresses[reads++] = address;
  /* Reproduce the transition's unmapped source: it has no ROM pointer. */
  if (address >= 0x0e2000 && address < 0x0e6000) return latch;
  CHECK((address >> 16) == 0x7f);
  return memory[(uint16_t)address];
}
static void write_bus(void *ctx, uint8_t reg, uint8_t value) {
  (void)ctx;
  CHECK(writes < 16); registers[writes] = reg; output[writes++] = value;
}
int main(void) {
  FzeroHdmaBus bus = {.read = read_bus, .write = write_bus};
  FzeroHdma c = {.bank = 0x7f, .table = 0x100, .indirect_bank = 0x0e,
                 .indirect = true, .mode = 2, .reg = 0x0f, .active = true};
  memory[0x100] = 0x82; memory[0x101] = 0; memory[0x102] = 0x20;
  FzeroHdmaLine(&c, &bus); FzeroHdmaLine(&c, &bus); FzeroHdmaLine(&c, &bus);
  CHECK(writes == 4 && !c.active);
  CHECK(addresses[3] == 0x0e2000 && addresses[6] == 0x0e2003);
  for (unsigned i = 0; i < writes; ++i) CHECK(output[i] == 0x20 && registers[i] == 0x0f);

  memset(memory, 0, sizeof(memory)); reads = writes = 0;
  c = (FzeroHdma){.bank = 0x7f, .table = 0xfffe, .mode = 1, .reg = 0x0d, .active = true};
  memory[0xfffe] = 2; memory[0xffff] = 0x12; memory[0] = 0x34;
  FzeroHdmaLine(&c, &bus);
  CHECK(writes == 2 && output[0] == 0x12 && output[1] == 0x34);
  CHECK(addresses[2] == 0x7f0000 && c.table == 1);
  CHECK(registers[0] == 0x0d && registers[1] == 0x0e);
  FzeroHdmaLine(&c, &bus); CHECK(writes == 2); /* non-repeating hold */
  FzeroHdmaLine(&c, &bus); CHECK(!c.active && writes == 2);
  puts("HDMA bus sources, repeat/hold, termination and bank wrapping: PASS");
  return 0;
}
