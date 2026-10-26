#pragma once

// Mock GNSS receiver: emits 1 Hz NMEA (RMC, VTG, GGA, talker GN) as if it were
// a NEO-M8N carried along MockRoute. Models cold start, position noise with a
// slowly drifting bias, varying satellites/HDOP and periodic fix outages.

#include <cstdint>

#include "hal.h"
#include "mock_route.h"
#include "nmea_writer.h"
#include "prng.h"

namespace hal {

struct MockGnssConfig {
  int64_t startUtcMs = 1793613600000LL;  // 2026-11-02T10:00:00Z
  uint32_t timeAfterS = 2;               // receiver knows UTC after this
  uint32_t coldStartS = 10;              // first fix after this
  double speedMps = 1.2;                 // walking
  float noiseSigmaM = 1.5f;              // white position noise per axis
  float biasStepM = 0.05f;               // random-walk bias per second
  float biasLimitM = 2.0f;
  uint32_t outagePeriodS = 180;          // a fix outage every period (0 = never)
  uint32_t outageDurationS = 12;
  float altitudeM = 938.0f;
  uint32_t seed = 1;
  bool silent = false;                   // simulate a disconnected module
};

class MockGnss : public GnssSerial {
 public:
  MockGnss(Clock& clock, const MockRoute& route, const MockGnssConfig& cfg = MockGnssConfig());

  bool begin(uint32_t baud) override;
  size_t read(char* buf, size_t maxLen) override;

  // Ground truth at the given epoch second (for tests/analysis).
  RoutePose truthAt(uint32_t epochSecond) const;
  // True if the receiver reports a fix at this epoch second.
  bool hasFixAt(uint32_t epochSecond) const;

 private:
  void generateEpoch(uint32_t second);

  Clock& clock_;
  const MockRoute& route_;
  MockGnssConfig cfg_;
  Prng rng_;
  bool started_ = false;
  int64_t nextEpoch_ = 0;
  float biasE_ = 0.0f, biasN_ = 0.0f;
  char buf_[640];
  size_t len_ = 0, pos_ = 0;
};

}  // namespace hal
