// HAL factory for real hardware (USE_MOCK=0).
#if !USE_MOCK

#include <Wire.h>

#include "drivers/real_devices.h"
#include "hal.h"
#include "pins.h"

namespace hal {

Devices& devices() {
  static bool busReady = [] {
    Wire.begin(pins::kI2cSda, pins::kI2cScl, 400000);
    Wire.setTimeOut(20);  // ms: a stuck sensor cannot block a task for long
    return true;
  }();
  (void)busReady;

  static drivers::EspClock clock;
  static drivers::UartGnss gnss(Serial2);
  static drivers::Mpu6050Imu imu;
  static drivers::Bme280Env env;
  static drivers::Vl53l0xRange range;
  static drivers::SdStorage storage;
  static Devices d{clock, gnss, imu, env, range, storage};
  return d;
}

}  // namespace hal

#endif  // !USE_MOCK
