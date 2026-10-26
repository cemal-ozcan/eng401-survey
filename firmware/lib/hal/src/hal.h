#pragma once

// Hardware abstraction layer. The application only talks to these interfaces;
// src/app/hal_real.cpp (USE_MOCK=0) or src/app/hal_mock.cpp (USE_MOCK=1)
// provides the implementations. Nothing outside the HAL may include a sensor
// library. All calls are short and non-blocking (bounded I2C/UART/SPI time).

#include <cstddef>
#include <cstdint>

namespace hal {

enum class Status : uint8_t {
  Ok,
  NotReady,   // no new data yet (non-blocking poll)
  NotFound,   // device did not answer at init
  ReadError,  // bus error / implausible data
};

const char* statusText(Status s);

// Monotonic microsecond clock (esp_timer on the device, fake in tests).
class Clock {
 public:
  virtual ~Clock() = default;
  virtual uint64_t nowUs() = 0;
};

// Raw NMEA byte stream from the GNSS receiver (UART).
class GnssSerial {
 public:
  virtual ~GnssSerial() = default;
  virtual bool begin(uint32_t baud) = 0;
  // Copies up to maxLen available bytes, never waits. Returns count.
  virtual size_t read(char* buf, size_t maxLen) = 0;
};

struct ImuSample {
  float ax, ay, az;  // m/s^2
  float gx, gy, gz;  // deg/s
  float tempC;
};

class Imu {
 public:
  virtual ~Imu() = default;
  virtual Status begin() = 0;
  virtual Status read(ImuSample& out) = 0;
};

struct EnvSample {
  float tempC;
  float pressureHpa;
  float humidityPct;
};

class EnvSensor {
 public:
  virtual ~EnvSensor() = default;
  virtual Status begin() = 0;
  virtual Status read(EnvSample& out) = 0;
};

struct RangeSample {
  uint16_t mm;
  bool valid;  // false: no target / out of range
};

class RangeSensor {
 public:
  virtual ~RangeSensor() = default;
  virtual Status begin() = 0;
  // Ok with a new sample, NotReady while a measurement is in progress.
  virtual Status poll(RangeSample& out) = 0;
};

// Append-only file storage (microSD).
class Storage {
 public:
  virtual ~Storage() = default;
  virtual bool begin() = 0;
  virtual bool exists(const char* path) = 0;
  virtual bool open(const char* path) = 0;  // create or append
  virtual size_t write(const char* data, size_t len) = 0;
  virtual bool flush() = 0;
  virtual void close() = 0;
  virtual uint64_t freeBytes() = 0;
};

struct Devices {
  Clock& clock;
  GnssSerial& gnss;
  Imu& imu;
  EnvSensor& env;
  RangeSensor& range;
  Storage& storage;
};

// Implemented by hal_real.cpp or hal_mock.cpp (selected by USE_MOCK).
Devices& devices();

}  // namespace hal
