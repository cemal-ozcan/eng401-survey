#include "shared_state.h"

#include <Arduino.h>

#include <cstdarg>

#include "config.h"

namespace app {

uint32_t SharedState::config_period_us() { return config::kSamplePeriodMs * 1000u; }

void SharedState::init() { mutex_ = xSemaphoreCreateMutex(); }

void SharedState::putGnss(const nmea::GnssData& d, uint64_t updateUs, bool timeSample) {
  lock();
  gnss_ = d;
  gnssUpdateUs_ = updateUs;
  if (timeSample) timeBase_.addSample(d.utcMs(), updateUs);
  unlock();
}

void SharedState::getGnss(nmea::GnssData& d, uint64_t& updateUs) {
  lock();
  d = gnss_;
  updateUs = gnssUpdateUs_;
  unlock();
}

int64_t SharedState::utcAt(uint64_t localUs) {
  lock();
  int64_t utc = timeBase_.toUtcMs(localUs);
  unlock();
  return utc;
}

rt::TimeBase SharedState::timeBase() {
  lock();
  rt::TimeBase copy = timeBase_;
  unlock();
  return copy;
}

void SharedState::addImu(const hal::ImuSample* s) {
  if (s == nullptr) return;
  lock();
  const float v[7] = {s->ax, s->ay, s->az, s->gx, s->gy, s->gz, s->tempC};
  for (int i = 0; i < 7; ++i) imuSum_[i] += v[i];
  ++imuN_;
  unlock();
}

void SharedState::setEnv(const hal::EnvSample* s) {
  lock();
  latest_.envOk = s != nullptr;
  if (s) latest_.env = *s;
  unlock();
}

void SharedState::setRange(const hal::RangeSample* s) {
  lock();
  latest_.rangeOk = s != nullptr;
  if (s) latest_.range = *s;
  unlock();
}

void SharedState::setHealth(const SensorHealth& h) {
  lock();
  health_ = h;
  unlock();
}

SensorHealth SharedState::health() {
  lock();
  SensorHealth h = health_;
  unlock();
  return h;
}

SensorSnapshot SharedState::takeSensors() {
  lock();
  SensorSnapshot s = latest_;
  s.imuOk = imuN_ > 0;
  if (imuN_ > 0) {
    const double n = imuN_;
    s.imu = {static_cast<float>(imuSum_[0] / n), static_cast<float>(imuSum_[1] / n),
             static_cast<float>(imuSum_[2] / n), static_cast<float>(imuSum_[3] / n),
             static_cast<float>(imuSum_[4] / n), static_cast<float>(imuSum_[5] / n),
             static_cast<float>(imuSum_[6] / n)};
  }
  for (double& v : imuSum_) v = 0;
  imuN_ = 0;
  latest_.rangeOk = false;  // each range sample is used once
  unlock();
  return s;
}

void SharedState::tickStats(uint64_t nowUs, uint32_t& missedOut) {
  lock();
  stats_.tick(nowUs);
  missedOut = stats_.missed();
  unlock();
}

rt::IntervalStats SharedState::stats() {
  lock();
  rt::IntervalStats copy = stats_;
  unlock();
  return copy;
}

void serialPrintf(const char* fmt, ...) {
  char buf[384];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  xSemaphoreTake(serialMutex, portMAX_DELAY);
  Serial.println(buf);
  xSemaphoreGive(serialMutex);
}

}  // namespace app
