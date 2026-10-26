// HAL factory for simulated hardware (USE_MOCK=1): Wokwi or a bare ESP32.
#if USE_MOCK

#include "drivers/esp_clock.h"
#include "hal.h"
#include "mock_gnss.h"
#include "mock_route.h"
#include "mock_sensors.h"
#include "mock_storage.h"

namespace hal {

Devices& devices() {
  static drivers::EspClock clock;
  static MockRoute route;
  static MockGnss gnss(clock, route);
  static MockImu imu(clock, 11);
  static MockEnv env(clock, 12);
  static MockRange range(clock, 13);
  static MockStorage storage(256 * 1024);
  static Devices d{clock, gnss, imu, env, range, storage};
  return d;
}

}  // namespace hal

#endif  // USE_MOCK
