#pragma once

// Sensor health supervision: after several consecutive read errors a sensor is
// declared failed and re-initialised with exponential back-off (1 s, 2 s, 4 s
// ... 30 s), so a loose connector recovers without rebooting the device.

#include <cstdint>

#include "hal.h"

namespace rt {

class Supervisor {
 public:
  static constexpr uint32_t kFailThreshold = 3;
  static constexpr uint64_t kMinBackoffUs = 1000000;
  static constexpr uint64_t kMaxBackoffUs = 30000000;

  enum class State : uint8_t { Ok, Failed };

  // Report the result of begin() or a read.
  void onInit(hal::Status s, uint64_t nowUs);
  void onRead(hal::Status s, uint64_t nowUs);

  // True when the caller should call begin() again now.
  bool retryDue(uint64_t nowUs) const { return state_ == State::Failed && nowUs >= nextRetryUs_; }

  State state() const { return state_; }
  bool ok() const { return state_ == State::Ok; }
  uint32_t errors() const { return totalErrors_; }
  uint32_t reinits() const { return reinits_; }

 private:
  void fail(uint64_t nowUs);

  State state_ = State::Failed;
  uint32_t consecutive_ = 0;
  uint32_t totalErrors_ = 0;
  uint32_t reinits_ = 0;
  uint64_t backoffUs_ = kMinBackoffUs;
  uint64_t nextRetryUs_ = 0;
  bool everInit_ = false;
};

}  // namespace rt
