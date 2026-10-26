#pragma once

// Real hardware implementations of the HAL interfaces (USE_MOCK=0 only).
// This is the ONLY place that includes sensor libraries.

#include <Adafruit_BME280.h>
#include <Adafruit_MPU6050.h>
#include <HardwareSerial.h>
#include <VL53L0X.h>

#include "esp_clock.h"
#include "hal.h"

namespace drivers {

class UartGnss : public hal::GnssSerial {
 public:
  explicit UartGnss(HardwareSerial& port) : port_(port) {}
  bool begin(uint32_t baud) override;
  size_t read(char* buf, size_t maxLen) override;

 private:
  HardwareSerial& port_;
};

class Mpu6050Imu : public hal::Imu {
 public:
  hal::Status begin() override;
  hal::Status read(hal::ImuSample& out) override;

 private:
  Adafruit_MPU6050 mpu_;
  bool ready_ = false;
};

class Bme280Env : public hal::EnvSensor {
 public:
  hal::Status begin() override;
  hal::Status read(hal::EnvSample& out) override;

 private:
  Adafruit_BME280 bme_;
  bool ready_ = false;
};

class Vl53l0xRange : public hal::RangeSensor {
 public:
  hal::Status begin() override;
  hal::Status poll(hal::RangeSample& out) override;

 private:
  VL53L0X tof_;
  bool ready_ = false;
};

class SdStorage : public hal::Storage {
 public:
  bool begin() override;
  bool exists(const char* path) override;
  bool open(const char* path) override;
  size_t write(const char* data, size_t len) override;
  bool flush() override;
  void close() override;
  uint64_t freeBytes() override;
};

}  // namespace drivers
