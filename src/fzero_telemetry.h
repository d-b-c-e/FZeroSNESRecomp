#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FzeroTelemetryState {
  uint16_t previous_x, previous_y;
  float total_distance_m;
  float speed_mps;
  uint32_t timestamp_ms;
  int have_position;
} FzeroTelemetryState;

/* Build one Forza Horizon 324-byte packet from a coherent post-simulation
 * WRAM snapshot.  Exposed for deterministic tests and alternate transports. */
size_t FzeroTelemetryBuild(FzeroTelemetryState *state, const uint8_t *ram,
                           size_t ram_size, uint32_t input,
                           uint8_t out[324]);

/* config.ini [Telemetry]: Enabled, Host, Port.  A missing/disabled section is
 * a no-op.  Network failure never stops the game. */
void FzeroTelemetryInit(const char *config_path);
void FzeroTelemetryFrame(const uint8_t *ram, size_t ram_size, uint32_t input);
void FzeroTelemetryShutdown(void);

#ifdef __cplusplus
}
#endif
