#include "timebase.h"

namespace rt {

void TimeBase::addSample(int64_t utcMs, uint64_t localUs) {
  if (utcMs < 0) return;
  const int64_t d = utcMs * 1000 - static_cast<int64_t>(localUs);
  if (count_ > 0) {
    int64_t delta = d - offsetUs_;
    if (delta > kJumpThresholdUs || delta < -kJumpThresholdUs) {
      // Receiver time jumped (e.g. leap of a wrong cold-start time): start over.
      count_ = 0;
      next_ = 0;
      ++resets_;
    }
  }
  window_[next_] = d;
  next_ = (next_ + 1) % kWindow;
  if (count_ < kWindow) ++count_;
  ++total_;
  recompute();
}

void TimeBase::recompute() {
  int64_t best = window_[(next_ - 1 + kWindow) % kWindow];
  for (int i = 0; i < count_; ++i)
    if (window_[i] > best) best = window_[i];
  offsetUs_ = best;
}

int64_t TimeBase::spreadUs() const {
  if (count_ == 0) return 0;
  int64_t lo = window_[0], hi = window_[0];
  for (int i = 1; i < count_; ++i) {
    if (window_[i] < lo) lo = window_[i];
    if (window_[i] > hi) hi = window_[i];
  }
  return hi - lo;
}

int64_t TimeBase::toUtcMs(uint64_t localUs) const {
  if (!synced()) return -1;
  int64_t utcUs = static_cast<int64_t>(localUs) + offsetUs_;
  return utcUs < 0 ? -1 : utcUs / 1000;
}

}  // namespace rt
