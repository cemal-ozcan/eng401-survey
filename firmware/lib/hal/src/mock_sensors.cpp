#include "mock_sensors.h"

#include <cmath>

namespace hal {

namespace {

bool faulted(Prng& rng, const MockFaults& f, uint64_t nowUs) {
  if (f.failAfterS != 0 && nowUs >= static_cast<uint64_t>(f.failAfterS) * 1000000ULL) return true;
  return rng.chance(f.readErrorRate);
}

double seconds(uint64_t us) { return static_cast<double>(us) * 1e-6; }

}  // namespace

// ---------------------------------------------------------------- IMU

MockImu::MockImu(Clock& clock, uint32_t seed, MockFaults faults, bool walking)
    : clock_(clock), rng_(seed), faults_(faults), walking_(walking) {
  for (float& b : biasG_) b = 0.5f * rng_.gauss();  // per-unit gyro bias, deg/s
}

Status MockImu::begin() {
  ready_ = !faults_.notFound;
  return ready_ ? Status::Ok : Status::NotFound;
}

Status MockImu::read(ImuSample& s) {
  if (!ready_) return Status::NotFound;
  uint64_t now = clock_.nowUs();
  if (faulted(rng_, faults_, now)) return Status::ReadError;
  double t = seconds(now);
  // Walking: vertical bounce at ~1.8 steps/s plus small sway.
  float bounce = walking_ ? 0.8f * static_cast<float>(std::sin(2.0 * 3.14159265 * 1.8 * t)) : 0.0f;
  float sway = walking_ ? 0.3f * static_cast<float>(std::sin(2.0 * 3.14159265 * 0.9 * t)) : 0.0f;
  s.ax = 0.05f + sway + 0.03f * rng_.gauss();
  s.ay = -0.04f + 0.03f * rng_.gauss();
  s.az = 9.81f + bounce + 0.03f * rng_.gauss();
  s.gx = biasG_[0] + 0.2f * rng_.gauss();
  s.gy = biasG_[1] + 0.2f * rng_.gauss();
  s.gz = biasG_[2] + 0.2f * rng_.gauss() + (walking_ ? 2.0f * sway : 0.0f);
  s.tempC = 27.0f + 0.1f * rng_.gauss();
  return Status::Ok;
}

// ---------------------------------------------------------------- BME280

MockEnv::MockEnv(Clock& clock, uint32_t seed, MockFaults faults)
    : clock_(clock), rng_(seed), faults_(faults) {}

Status MockEnv::begin() {
  ready_ = !faults_.notFound;
  return ready_ ? Status::Ok : Status::NotFound;
}

Status MockEnv::read(EnvSample& s) {
  if (!ready_) return Status::NotFound;
  uint64_t now = clock_.nowUs();
  if (faulted(rng_, faults_, now)) return Status::ReadError;
  double t = seconds(now);
  // Slow drift (sun/shade along the route) + sensor noise (datasheet-like).
  s.tempC = 18.0f + 1.5f * static_cast<float>(std::sin(t / 240.0)) + 0.02f * rng_.gauss();
  s.pressureHpa = 912.0f + 0.3f * static_cast<float>(std::sin(t / 90.0)) + 0.05f * rng_.gauss();
  s.humidityPct = 41.0f - 3.0f * static_cast<float>(std::sin(t / 240.0)) + 0.3f * rng_.gauss();
  return Status::Ok;
}

// ---------------------------------------------------------------- VL53L0X

MockRange::MockRange(Clock& clock, uint32_t seed, MockFaults faults, uint32_t periodMs, float outOfRangeRate,
                     float terrainAmplitudeMm)
    : clock_(clock),
      rng_(seed),
      faults_(faults),
      periodUs_(periodMs * 1000u),
      outOfRangeRate_(outOfRangeRate),
      terrainMm_(terrainAmplitudeMm) {}

Status MockRange::begin() {
  ready_ = !faults_.notFound;
  nextUs_ = clock_.nowUs() + periodUs_;
  return ready_ ? Status::Ok : Status::NotFound;
}

Status MockRange::poll(RangeSample& s) {
  if (!ready_) return Status::NotFound;
  uint64_t now = clock_.nowUs();
  if (now < nextUs_) return Status::NotReady;
  nextUs_ = now + periodUs_;
  if (faulted(rng_, faults_, now)) return Status::ReadError;
  if (rng_.chance(outOfRangeRate_)) {
    s.mm = 0;
    s.valid = false;
    return Status::Ok;
  }
  // Sensor on the pole pointing at the ground ~1.5 m away; terrain varies slowly.
  double t = seconds(now);
  float mm = 1500.0f + terrainMm_ * static_cast<float>(std::sin(t / 7.0)) + 4.0f * rng_.gauss();
  s.mm = static_cast<uint16_t>(mm);
  s.valid = true;
  return Status::Ok;
}

}  // namespace hal
