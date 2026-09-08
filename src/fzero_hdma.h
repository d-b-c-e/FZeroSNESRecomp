#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Deferred HDMA retains guest addresses: a valid transfer may read unmapped
 * bus space and must not require every source to have a host ROM pointer. */
typedef struct FzeroHdmaBus {
  uint8_t (*read)(void *context, uint32_t address, uint8_t open_bus);
  void (*write)(void *context, uint8_t reg, uint8_t value);
  void *context;
  uint8_t latch;
} FzeroHdmaBus;

typedef struct FzeroHdma {
  uint16_t table, indirect_address;
  uint8_t bank, indirect_bank, mode, reg, count;
  bool indirect, active;
} FzeroHdma;

void FzeroHdmaLine(FzeroHdma *channel, FzeroHdmaBus *bus);
