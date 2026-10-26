#pragma once

// Requested vs actual sampling interval (handout section 9): records the time
// between consecutive ticks, counts missed ticks, keeps min/max/mean/std.

#include <cstdint>

namespace rt {

class IntervalStats {
 public:
  explicit IntervalStats(uint32_t periodUs) : periodUs_(periodUs) {}

  // Call once per executed tick with its timestamp.
  void tick(uint64_t nowUs);

  uint32_t periodUs() const { return periodUs_; }
  uint32_t intervals() const { return n_; }
  uint32_t missed() const { return missed_; }
  double meanUs() const { return n_ ? mean_ : 0.0; }
  double stdUs() const;
  uint64_t minUs() const { return n_ ? min_ : 0; }
  uint64_t maxUs() const { return max_; }
  void reset();

 private:
  uint32_t periodUs_;
  bool started_ = false;
  uint64_t last_ = 0;
  uint32_t n_ = 0;
  uint32_t missed_ = 0;
  double mean_ = 0.0, m2_ = 0.0;  // Welford
  uint64_t min_ = 0, max_ = 0;
};

}  // namespace rt
