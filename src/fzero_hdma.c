#include "fzero_hdma.h"

static uint8_t read_byte(FzeroHdmaBus *bus, uint8_t bank, uint16_t *address) {
  bus->latch = bus->read(bus->context, ((uint32_t)bank << 16) | *address, bus->latch);
  ++*address; /* HDMA address increments wrap within the selected bank. */
  return bus->latch;
}

void FzeroHdmaLine(FzeroHdma *c, FzeroHdmaBus *bus) {
  static const uint8_t lengths[8] = {1,2,2,4,4,4,2,4};
  static const uint8_t offsets[8][4] = {
    {0,0,0,0},{0,1,0,1},{0,0,0,0},{0,0,1,1},
    {0,1,2,3},{0,1,0,1},{0,0,0,0},{0,0,1,1}};
  if (!c->active) return;
  bool transfer = false;
  if (!(c->count & 0x7f)) {
    c->count = read_byte(bus, c->bank, &c->table);
    if (!c->count) { c->active = false; return; }
    if (c->indirect) {
      uint8_t lo = read_byte(bus, c->bank, &c->table);
      uint8_t hi = read_byte(bus, c->bank, &c->table);
      c->indirect_address = (uint16_t)lo | ((uint16_t)hi << 8);
    }
    transfer = true;
  }
  if (transfer || (c->count & 0x80)) {
    unsigned mode = c->mode & 7;
    for (unsigned i = 0; i < lengths[mode]; ++i) {
      uint8_t value = read_byte(bus, c->indirect ? c->indirect_bank : c->bank,
                               c->indirect ? &c->indirect_address : &c->table);
      bus->write(bus->context, (uint8_t)(c->reg + offsets[mode][i]), value);
    }
  }
  --c->count;
}
