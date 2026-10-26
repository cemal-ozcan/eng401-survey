#include "esp_clock.h"

#include <esp_timer.h>

namespace drivers {

uint64_t EspClock::nowUs() { return static_cast<uint64_t>(esp_timer_get_time()); }

}  // namespace drivers
