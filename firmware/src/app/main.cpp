// Survey firmware, step 3: all sensors through the HAL, one CSV record per second
// on the serial port. Identical code for real (esp32) and simulated (esp32-mock)
// hardware. Step 4 moves this into FreeRTOS tasks with queues.

#include <Arduino.h>

#include "build_info.h"
#include "hal.h"
#include "nmea_parser.h"
#include "record.h"
#include "record_builder.h"

namespace {

constexpr uint32_t kGnssBaud = 9600;  // u-blox default (NEO-6M / NEO-M8N)
constexpr uint32_t kTickMs = 100;
constexpr uint32_t kTicksPerRecord = 10;  // 1 Hz records

nmea::Parser gnssParser;
uint64_t gnssUpdateUs = 0;
hal::RangeSample lastRange{0, false};
bool rangeFresh = false;
uint32_t seq = 0;

void printInit(const char* name, hal::Status s) {
  Serial.printf("  %-8s %s\n", name, hal::statusText(s));
}

void drainGnss(hal::Devices& d) {
  char buf[128];
  size_t n;
  while ((n = d.gnss.read(buf, sizeof(buf))) > 0) {
    for (size_t i = 0; i < n; ++i) {
      nmea::Sentence s = gnssParser.feed(buf[i]);
      if (s == nmea::Sentence::GGA || s == nmea::Sentence::RMC) gnssUpdateUs = d.clock.nowUs();
    }
  }
}

void emitRecord(hal::Devices& d) {
  record::Record r;
  r.seq = seq++;
  r.tUs = d.clock.nowUs();
  r.utcMs = gnssParser.data().utcMs();
  if (build_info::isMock()) r.flags |= record::kFlagMock;
  record::fillGnss(r, gnssParser.data(), gnssUpdateUs, r.tUs);

  hal::ImuSample imu;
  record::fillImu(r, d.imu.read(imu) == hal::Status::Ok ? &imu : nullptr);
  hal::EnvSample env;
  record::fillEnv(r, d.env.read(env) == hal::Status::Ok ? &env : nullptr);
  record::fillRange(r, rangeFresh ? &lastRange : nullptr);
  rangeFresh = false;

  char line[record::kMaxCsvLen];
  if (record::toCsv(r, line, sizeof(line)) > 0) Serial.println(line);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);  // let the serial monitor attach; setup only, before the survey starts
  Serial.printf("\nENG401 survey firmware v%s (%s sensors)\n", build_info::version(),
                build_info::modeName());

  hal::Devices& d = hal::devices();
  Serial.println("Device init:");
  printInit("GNSS", d.gnss.begin(kGnssBaud) ? hal::Status::Ok : hal::Status::NotFound);
  printInit("MPU6050", d.imu.begin());
  printInit("BME280", d.env.begin());
  printInit("VL53L0X", d.range.begin());
  printInit("SD", d.storage.begin() ? hal::Status::Ok : hal::Status::NotFound);
  Serial.println(record::csvHeader());
}

void loop() {
  static TickType_t lastWake = xTaskGetTickCount();
  static uint32_t tick = 0;
  hal::Devices& d = hal::devices();

  drainGnss(d);
  hal::RangeSample rs;
  if (d.range.poll(rs) == hal::Status::Ok) {
    lastRange = rs;
    rangeFresh = true;
  }
  if (++tick % kTicksPerRecord == 0) emitRecord(d);

  vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kTickMs));  // fixed-rate, not delay()
}
