#include "interval_stats.h"

#include <cmath>

namespace rt {

void IntervalStats::tick(uint64_t now) {
  if (!started_) {
    started_ = true;
    last_ = now;
    return;
  }
  const uint64_t dt = now - last_;
  last_ = now;
  // A gap of k periods (rounded) means k-1 ticks were skipped.
  const uint64_t periods = (dt + periodUs_ / 2) / periodUs_;
  if (periods > 1) missed_ += static_cast<uint32_t>(periods - 1);

  ++n_;
  const double x = static_cast<double>(dt);
  const double delta = x - mean_;
  mean_ += delta / n_;
  m2_ += delta * (x - mean_);
  if (n_ == 1 || dt < min_) min_ = dt;
  if (dt > max_) max_ = dt;
}

double IntervalStats::stdUs() const { return n_ > 1 ? std::sqrt(m2_ / (n_ - 1)) : 0.0; }

void IntervalStats::reset() {
  uint32_t p = periodUs_;
  *this = IntervalStats(p);
}

}  // namespace rt
