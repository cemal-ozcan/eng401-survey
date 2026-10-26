#pragma once

// Push-button debouncer: a level must be stable for `stableUs` to count.
// pressed() is true exactly once per press (falling edge of an active-low button).

#include <cstdint>

namespace rt {

class Debouncer {
 public:
  explicit Debouncer(uint64_t stableUs = 30000) : stableUs_(stableUs) {}

  // rawPressed = current electrical state. Returns true on a debounced press.
  bool update(bool rawPressed, uint64_t nowUs) {
    if (rawPressed != lastRaw_) {
      lastRaw_ = rawPressed;
      changedUs_ = nowUs;
      return false;
    }
    if (rawPressed != stable_ && nowUs - changedUs_ >= stableUs_) {
      stable_ = rawPressed;
      return stable_;
    }
    return false;
  }

  bool isPressed() const { return stable_; }

 private:
  uint64_t stableUs_;
  bool lastRaw_ = false;
  bool stable_ = false;
  uint64_t changedUs_ = 0;
};

}  // namespace rt
