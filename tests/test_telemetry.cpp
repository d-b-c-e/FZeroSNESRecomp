#include "fzero_telemetry.h"
#include "forza_packet.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static void put16(uint8_t *p, uint16_t value) {
  p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
}

int main() {
  uint8_t ram[0x20000]{};
  uint8_t packet[324]{};
  FzeroTelemetryState state{};
  ram[0x54] = 2; ram[0x55] = 3;
  put16(ram + 0x0b70, 0x1ffe);
  put16(ram + 0x0b90, 100);
  CHECK(FzeroTelemetryBuild(&state, ram, sizeof(ram), 0, packet) == 324);
  CHECK(packet[323] == 'F');

  /* Cross the X wrap: 0x1ffe -> 2 is four units, not an 8192-unit jump. */
  put16(ram + 0x0b70, 2);
  CHECK(FzeroTelemetryBuild(&state, ram, sizeof(ram), 0x0081, packet) == 324);
  float speed = 0;
  std::memcpy(&speed, packet + dbce::forza::OFF_SPEED_HORIZON, sizeof(speed));
  CHECK(speed > 0.0f && speed < 100.0f);
  CHECK(packet[244 + 71] == 255); /* accel */
  CHECK((int8_t)packet[dbce::forza::OFF_STEER_HORIZON] == 127);
  CHECK(packet[dbce::forza::OFF_GEAR_HORIZON] == 1);

  ram[0x55] = 0;
  CHECK(FzeroTelemetryBuild(&state, ram, sizeof(ram), 0, packet) == 324);
  std::memcpy(&speed, packet + dbce::forza::OFF_SPEED_HORIZON, sizeof(speed));
  CHECK(speed == 0.0f && state.have_position == 0);
  CHECK(FzeroTelemetryBuild(&state, ram, 16, 0, packet) == 0);

#ifdef _WIN32
  /* Exercise the configured transport as well as the pure serializer. */
  WSADATA wsa{};
  CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
  SOCKET receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  CHECK(receiver != INVALID_SOCKET);
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  CHECK(bind(receiver, (sockaddr *)&local, sizeof(local)) == 0);
  int local_size = sizeof(local);
  CHECK(getsockname(receiver, (sockaddr *)&local, &local_size) == 0);
  DWORD timeout_ms = 1000;
  CHECK(setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout_ms, sizeof(timeout_ms)) == 0);

  const char *config_path = "fzero_telemetry_test.ini";
  FILE *config = std::fopen(config_path, "wb");
  CHECK(config != nullptr);
  std::fprintf(config, "[Telemetry]\nEnabled=1\nHost=127.0.0.1\nPort=%u\n",
               (unsigned)ntohs(local.sin_port));
  std::fclose(config);
  FzeroTelemetryInit(config_path);
  FzeroTelemetryFrame(ram, sizeof(ram), 0x0001);
  uint8_t received[324]{};
  CHECK(recv(receiver, (char *)received, sizeof(received), 0) == 324);
  CHECK(received[323] == 'F');
  FzeroTelemetryShutdown();
  closesocket(receiver);
  WSACleanup();
  std::remove(config_path);
#endif

  std::puts("F-Zero Forza telemetry packet tests passed");
  return 0;
}
