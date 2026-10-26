#pragma once

// Mock IMU, environmental and range sensors with realistic noise and
// injectable faults, so the error handling can be tested without hardware.

#include <cstdint>

#include "hal.h"
#include "prng.h"

namespace hal {

struct MockFaults {
  bool notFound = false;       // begin() fails
  float readErrorRate = 0.0f;  // probability that a read fails
  uint32_t failAfterS = 0;     // all reads fail after this time (0 = never), e.g. loose wire
};

class MockImu : public Imu {
 public:
  MockImu(Clock& clock, uint32_t seed, MockFaults faults = MockFaults(), bool walking = true);
  Status begin() override;
  Status read(ImuSample& out) override;

 private:
  Clock& clock_;
  Prng rng_;
  MockFaults faults_;
  bool walking_;
  bool ready_ = false;
  float biasG_[3];
};

class MockEnv : public EnvSensor {
 public:
  MockEnv(Clock& clock, uint32_t seed, MockFaults faults = MockFaults());
  Status begin() override;
  Status read(EnvSample& out) override;

 private:
  Clock& clock_;
  Prng rng_;
  MockFaults faults_;
  bool ready_ = false;
};

class MockRange : public RangeSensor {
 public:
  // Measures every periodMs; noOutOfRangeRate = probability of "no target".
  MockRange(Clock& clock, uint32_t seed, MockFaults faults = MockFaults(), uint32_t periodMs = 100,
            float outOfRangeRate = 0.05f);
  Status begin() override;
  Status poll(RangeSample& out) override;

 private:
  Clock& clock_;
  Prng rng_;
  MockFaults faults_;
  uint32_t periodUs_;
  float outOfRangeRate_;
  bool ready_ = false;
  uint64_t nextUs_ = 0;
};

}  // namespace hal
