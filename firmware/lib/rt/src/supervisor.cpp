#include "supervisor.h"

namespace rt {

void Supervisor::fail(uint64_t nowUs) {
  state_ = State::Failed;
  nextRetryUs_ = nowUs + backoffUs_;
  backoffUs_ = backoffUs_ * 2 > kMaxBackoffUs ? kMaxBackoffUs : backoffUs_ * 2;
}

void Supervisor::onInit(hal::Status s, uint64_t nowUs) {
  if (everInit_) ++reinits_;
  everInit_ = true;
  if (s == hal::Status::Ok) {
    state_ = State::Ok;
    consecutive_ = 0;
    backoffUs_ = kMinBackoffUs;
  } else {
    fail(nowUs);
  }
}

void Supervisor::onRead(hal::Status s, uint64_t nowUs) {
  if (s == hal::Status::Ok) {
    consecutive_ = 0;
    return;
  }
  if (s == hal::Status::NotReady) return;  // not an error
  ++totalErrors_;
  if (state_ == State::Ok && ++consecutive_ >= kFailThreshold) fail(nowUs);
}

}  // namespace rt
