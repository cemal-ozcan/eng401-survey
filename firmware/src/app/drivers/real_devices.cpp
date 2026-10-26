// Real sensor drivers, compiled only for USE_MOCK=0.
#if !USE_MOCK

#include "real_devices.h"

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#include <cmath>

#include "pins.h"

namespace drivers {

namespace {
constexpr uint32_t kTofPeriodMs = 100;
constexpr uint32_t kSdSpiHz = 4000000;
constexpr size_t kGnssRxBuffer = 1024;  // ~1 s of NMEA at 9600 baud
constexpr float kRadToDeg = 57.29578f;
File gFile;  // SD.h keeps file objects by value
}  // namespace

// ---------------------------------------------------------------- GNSS

bool UartGnss::begin(uint32_t baud) {
  port_.end();
  port_.setRxBufferSize(kGnssRxBuffer);  // must precede begin()
  port_.begin(baud, SERIAL_8N1, pins::kGnssRx, pins::kGnssTx);
  return true;
}

size_t UartGnss::read(char* buf, size_t maxLen) {
  size_t n = 0;
  while (n < maxLen && port_.available() > 0) buf[n++] = static_cast<char>(port_.read());
  return n;
}

// ---------------------------------------------------------------- MPU6050

hal::Status Mpu6050Imu::begin() {
  ready_ = mpu_.begin(0x68, &Wire) || mpu_.begin(0x69, &Wire);
  if (!ready_) return hal::Status::NotFound;
  mpu_.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu_.setFilterBandwidth(MPU6050_BAND_21_HZ);
  return hal::Status::Ok;
}

hal::Status Mpu6050Imu::read(hal::ImuSample& s) {
  if (!ready_) return hal::Status::NotFound;
  sensors_event_t a, g, t;
  if (!mpu_.getEvent(&a, &g, &t)) return hal::Status::ReadError;
  s.ax = a.acceleration.x;
  s.ay = a.acceleration.y;
  s.az = a.acceleration.z;
  s.gx = g.gyro.x * kRadToDeg;
  s.gy = g.gyro.y * kRadToDeg;
  s.gz = g.gyro.z * kRadToDeg;
  s.tempC = t.temperature;
  return hal::Status::Ok;
}

// ---------------------------------------------------------------- BME280

hal::Status Bme280Env::begin() {
  ready_ = bme_.begin(0x76, &Wire) || bme_.begin(0x77, &Wire);
  if (!ready_) return hal::Status::NotFound;
  // Normal mode: the chip measures continuously, reads never wait.
  bme_.setSampling(Adafruit_BME280::MODE_NORMAL, Adafruit_BME280::SAMPLING_X2,
                   Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X1,
                   Adafruit_BME280::FILTER_X4, Adafruit_BME280::STANDBY_MS_62_5);
  return hal::Status::Ok;
}

hal::Status Bme280Env::read(hal::EnvSample& s) {
  if (!ready_) return hal::Status::NotFound;
  s.tempC = bme_.readTemperature();
  s.pressureHpa = bme_.readPressure() / 100.0f;
  s.humidityPct = bme_.readHumidity();
  // A disconnected chip reads back as NAN or garbage: reject implausible values.
  bool ok = std::isfinite(s.tempC) && s.tempC > -40 && s.tempC < 85 && s.pressureHpa > 300 &&
            s.pressureHpa < 1100 && s.humidityPct >= 0 && s.humidityPct <= 100;
  return ok ? hal::Status::Ok : hal::Status::ReadError;
}

// ---------------------------------------------------------------- VL53L0X

hal::Status Vl53l0xRange::begin() {
  pinMode(pins::kTofXshut, OUTPUT);
  digitalWrite(pins::kTofXshut, HIGH);
  tof_.setBus(&Wire);
  tof_.setTimeout(50);
  ready_ = tof_.init();
  if (!ready_) return hal::Status::NotFound;
  tof_.startContinuous(kTofPeriodMs);
  return hal::Status::Ok;
}

// Non-blocking replacement for readRangeContinuousMillimeters(), which waits
// for the measurement: check the interrupt flag and return NotReady if unset.
hal::Status Vl53l0xRange::poll(hal::RangeSample& s) {
  if (!ready_) return hal::Status::NotFound;
  uint8_t irq = tof_.readReg(VL53L0X::RESULT_INTERRUPT_STATUS);
  if (tof_.last_status != 0) return hal::Status::ReadError;
  if ((irq & 0x07) == 0) return hal::Status::NotReady;
  uint16_t mm = tof_.readReg16Bit(VL53L0X::RESULT_RANGE_STATUS + 10);
  tof_.writeReg(VL53L0X::SYSTEM_INTERRUPT_CLEAR, 0x01);
  if (tof_.last_status != 0) return hal::Status::ReadError;
  s.valid = mm > 0 && mm < 8190;  // 8190/8191 = no target
  s.mm = s.valid ? mm : 0;
  return hal::Status::Ok;
}

// ---------------------------------------------------------------- SD card

bool SdStorage::begin() {
  SPI.begin(pins::kSdSck, pins::kSdMiso, pins::kSdMosi, pins::kSdCs);
  return SD.begin(pins::kSdCs, SPI, kSdSpiHz) && SD.cardType() != CARD_NONE;
}

bool SdStorage::exists(const char* path) { return SD.exists(path); }

bool SdStorage::open(const char* path) {
  if (gFile) gFile.close();
  gFile = SD.open(path, FILE_APPEND);
  return static_cast<bool>(gFile);
}

size_t SdStorage::write(const char* data, size_t len) {
  if (!gFile) return 0;
  return gFile.write(reinterpret_cast<const uint8_t*>(data), len);
}

bool SdStorage::flush() {
  if (!gFile) return false;
  gFile.flush();
  return true;
}

void SdStorage::close() {
  if (gFile) gFile.close();
}

uint64_t SdStorage::freeBytes() { return SD.totalBytes() - SD.usedBytes(); }

}  // namespace drivers

#endif  // !USE_MOCK
