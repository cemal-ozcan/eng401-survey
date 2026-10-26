#pragma once

#include "hal.h"

namespace drivers {

// esp_timer: 64-bit microseconds since boot, monotonic. Shared by real and mock HAL.
class EspClock : public hal::Clock {
 public:
  uint64_t nowUs() override;
};

}  // namespace drivers
