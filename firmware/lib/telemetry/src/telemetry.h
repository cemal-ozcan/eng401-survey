#pragma once

// Telemetry packets other than 'data' (which record::toJson encodes), see
// docs/ICD.md section 3. Pure functions, no networking.

#include <cstddef>
#include <cstdint>

namespace telemetry {

constexpr uint16_t kPcPort = 5005;      // ESP32 -> PC
constexpr uint16_t kDevicePort = 5006;  // PC -> ESP32 (ping)

struct Heartbeat {
  uint64_t tUs;
  uint32_t uptimeS;
  uint32_t heap;
  int32_t rssi;        // dBm
  uint32_t sdFreeMb;
  uint32_t queueDrops;
  uint32_t sdErrors;
  uint32_t netDrops;   // records the net task could not send
  const char* fw;
};

size_t encodeHeartbeat(const Heartbeat& hb, char* out, size_t size);

struct Ping {
  uint32_t id;
  int64_t t1;  // PC clock, µs (opaque to the device, echoed back)
};

// Parses {"v":1,"type":"ping","id":7,"t1":123}. Returns false for anything else.
bool parsePing(const char* json, Ping& out);

// t2 = device clock at ping reception, t3 = device clock just before sending.
size_t encodePong(const Ping& p, uint64_t t2, uint64_t t3, char* out, size_t size);

// Minimal JSON integer lookup ("key":123) for flat objects. Exposed for tests.
bool jsonGetInt(const char* json, const char* key, int64_t& out);

}  // namespace telemetry
