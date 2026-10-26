// Acquisition tasks: GNSS UART parsing and sensor polling with supervision.

#include <Arduino.h>

#include "config.h"
#include "hal.h"
#include "shared_state.h"
#include "tasks.h"

namespace app {

void gnssTask(void*) {
  hal::Devices& d = hal::devices();
  nmea::Parser parser;
  char buf[128];
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    size_t n;
    while ((n = d.gnss.read(buf, sizeof(buf))) > 0) {
      for (size_t i = 0; i < n; ++i) {
        nmea::Sentence s = parser.feed(buf[i]);
        if (s != nmea::Sentence::GGA && s != nmea::Sentence::RMC) continue;
        // Reception time is quantised by kGnssPollMs; the time base keeps the
        // least-delayed sample, which compensates most of that.
        shared.putGnss(parser.data(), d.clock.nowUs(), parser.data().utcMs() >= 0);
      }
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(config::kGnssPollMs));
  }
}

namespace {

template <typename Sensor>
void superviseInit(Sensor& sensor, rt::Supervisor& sv, uint64_t now, const char* name) {
  hal::Status s = sensor.begin();
  sv.onInit(s, now);
  serialPrintf("# INIT %s %s", name, hal::statusText(s));
}

}  // namespace

void sensorTask(void*) {
  hal::Devices& d = hal::devices();
  SensorHealth h;
  const uint64_t t0 = d.clock.nowUs();
  superviseInit(d.imu, h.imu, t0, "MPU6050");
  superviseInit(d.env, h.env, t0, "BME280");
  superviseInit(d.range, h.range, t0, "VL53L0X");

  uint32_t loop = 0;
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    const uint64_t now = d.clock.nowUs();

    // Re-initialise failed sensors with back-off (loose connector recovery).
    if (h.imu.retryDue(now)) superviseInit(d.imu, h.imu, now, "MPU6050");
    if (h.env.retryDue(now)) superviseInit(d.env, h.env, now, "BME280");
    if (h.range.retryDue(now)) superviseInit(d.range, h.range, now, "VL53L0X");

    if (h.range.ok()) {
      hal::RangeSample rs;
      hal::Status s = d.range.poll(rs);
      h.range.onRead(s, now);
      if (s == hal::Status::Ok) shared.setRange(&rs);
    }
    if (loop % config::kImuEveryLoops == 0 && h.imu.ok()) {
      hal::ImuSample is;
      hal::Status s = d.imu.read(is);
      h.imu.onRead(s, now);
      shared.addImu(s == hal::Status::Ok ? &is : nullptr);
    }
    if (loop % config::kEnvEveryLoops == 0) {
      hal::EnvSample es;
      hal::Status s = h.env.ok() ? d.env.read(es) : hal::Status::NotFound;
      if (h.env.ok()) h.env.onRead(s, now);
      shared.setEnv(s == hal::Status::Ok ? &es : nullptr);
    }
    shared.setHealth(h);
    ++loop;
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(config::kSensorLoopMs));
  }
}

}  // namespace app
