#pragma once

// One survey record = one sampling tick. Field list, units and encodings are
// defined in docs/ICD.md; the PC codec (pc/surveypc/protocol.py) mirrors this.
//
// Invalid values: NAN for floating point, -1 for utcMs, 0 for utmZone,
// kAgeUnknown for gnssAgeMs. They encode as an empty CSV field / JSON null.

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace record {

constexpr int kProtocolVersion = 1;
constexpr uint32_t kAgeUnknown = 0xFFFFFFFFu;

// flags bitmask (ICD 2.1)
enum Flag : uint16_t {
  kFlagGnssFix = 1u << 0,
  kFlagImuOk = 1u << 1,
  kFlagEnvOk = 1u << 2,
  kFlagDistOk = 1u << 3,
  kFlagSdOk = 1u << 4,
  kFlagWifi = 1u << 5,
  kFlagUtcSynced = 1u << 6,
  kFlagMock = 1u << 7,
};

struct Record {
  uint32_t seq = 0;
  uint64_t tUs = 0;
  int64_t utcMs = -1;

  uint8_t fix = 0;
  uint8_t sats = 0;
  float hdop = NAN;
  double lat = NAN;
  double lon = NAN;
  float alt = NAN;
  float spd = NAN;
  float cog = NAN;
  uint32_t gnssAgeMs = kAgeUnknown;

  uint8_t utmZone = 0;
  char utmHemi = 'N';
  double easting = NAN;
  double northing = NAN;

  float ax = NAN, ay = NAN, az = NAN;  // m/s^2
  float gx = NAN, gy = NAN, gz = NAN;  // deg/s
  float temp = NAN;                    // degC
  float pres = NAN;                    // hPa
  float hum = NAN;                     // %RH
  uint16_t dist = 0;                   // mm, 0 = invalid

  uint16_t flags = 0;
  uint32_t missed = 0;
  uint16_t mark = 0;
};

// Comma-separated column names, no line ending.
const char* csvHeader();

// Encoders return the number of chars written (excluding NUL), or 0 if the
// buffer is too small. No line ending is appended.
constexpr size_t kMaxCsvLen = 320;
constexpr size_t kMaxJsonLen = 640;
size_t toCsv(const Record& r, char* out, size_t outSize);
size_t toJson(const Record& r, char* out, size_t outSize);

}  // namespace record
