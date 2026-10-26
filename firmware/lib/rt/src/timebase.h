#pragma once

// Common time reference (requirement R4): maps the monotonic esp_timer clock
// (local µs since boot) to UTC using GNSS time.
//
// Each decoded NMEA time gives one sample  d = utc_us - local_us_at_reception.
// Reception always lags the true epoch (UART transfer + receiver latency), so
// d underestimates the true offset; the sample with the SMALLEST lag gives the
// LARGEST d. The estimate is therefore the maximum d over a sliding window.
// Without a PPS line the residual error is the minimum lag (typically tens of ms
// for the first sentence of an epoch); with PPS it could be reduced to µs.

#include <cstdint>

namespace rt {

class TimeBase {
 public:
  static constexpr int kWindow = 32;               // samples (~32 s at 1 Hz)
  static constexpr int64_t kJumpThresholdUs = 2000000;  // receiver time reset

  // Feed one GNSS time sample: utcMs = time encoded in the sentence,
  // localUs = clock when the sentence was received.
  void addSample(int64_t utcMs, uint64_t localUs);

  bool synced() const { return count_ > 0; }
  // UTC (epoch ms) for a local timestamp; -1 if never synced.
  int64_t toUtcMs(uint64_t localUs) const;
  // Current offset estimate (utc_us - local_us) and spread of the window.
  int64_t offsetUs() const { return offsetUs_; }
  int64_t spreadUs() const;
  uint32_t samples() const { return total_; }
  uint32_t resets() const { return resets_; }

 private:
  void recompute();

  int64_t window_[kWindow] = {0};
  int count_ = 0;
  int next_ = 0;
  int64_t offsetUs_ = 0;
  uint32_t total_ = 0;
  uint32_t resets_ = 0;
};

}  // namespace rt
