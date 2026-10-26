#pragma once

// Data shared between tasks. Every access goes through one FreeRTOS mutex and
// copies small structs in/out, so critical sections stay a few microseconds.

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>

#include "hal.h"
#include "interval_stats.h"
#include "nmea_parser.h"
#include "record.h"
#include "supervisor.h"
#include "timebase.h"

namespace app {

struct SensorHealth {
  rt::Supervisor imu, env, range;
};

struct SensorSnapshot {
  bool imuOk = false;
  hal::ImuSample imu{};  // mean since the previous record
  bool envOk = false;
  hal::EnvSample env{};
  bool rangeOk = false;
  hal::RangeSample range{};
};

class SharedState {
 public:
  void init();

  // GNSS task
  void putGnss(const nmea::GnssData& d, uint64_t updateUs, bool timeSample);
  void getGnss(nmea::GnssData& d, uint64_t& updateUs);
  int64_t utcAt(uint64_t localUs);
  rt::TimeBase timeBase();

  // Sensor task
  void addImu(const hal::ImuSample* s);  // nullptr = failed read
  void setEnv(const hal::EnvSample* s);
  void setRange(const hal::RangeSample* s);
  void setHealth(const SensorHealth& h);
  SensorHealth health();

  // Sample task: takes the IMU mean and the latest env/range sample.
  SensorSnapshot takeSensors();
  void tickStats(uint64_t nowUs, uint32_t& missedOut);
  rt::IntervalStats stats();

  void lock() { xSemaphoreTake(mutex_, portMAX_DELAY); }
  void unlock() { xSemaphoreGive(mutex_); }

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  nmea::GnssData gnss_;
  uint64_t gnssUpdateUs_ = 0;
  rt::TimeBase timeBase_;
  double imuSum_[7] = {0};
  uint32_t imuN_ = 0;
  SensorSnapshot latest_;
  SensorHealth health_;
  rt::IntervalStats stats_{config_period_us()};

  static uint32_t config_period_us();
};

// Process-wide objects (defined in app_main.cpp)
extern SharedState shared;
extern QueueHandle_t logQueue;
extern QueueHandle_t netQueue;
extern SemaphoreHandle_t serialMutex;

// Counters readable from any task
extern std::atomic<uint32_t> queueDrops;
extern std::atomic<uint32_t> recordsProduced;
extern std::atomic<uint32_t> recordsLogged;
extern std::atomic<uint16_t> pendingMark;   // set by UI, consumed by sampler
extern std::atomic<uint64_t> lastLogUs;     // drives the blue LED
extern std::atomic<bool> storageOk;

// Serial output from several tasks: one line at a time.
void serialPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace app
