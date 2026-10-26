#include "mock_gnss.h"

#include <cmath>
#include <cstring>

namespace hal {

namespace {
constexpr int64_t kMsPerDay = 86400000LL;
constexpr uint32_t kMaxEpochsPerRead = 3;  // after a long pause, do not flood
}  // namespace

MockGnss::MockGnss(Clock& clock, const MockRoute& route, const MockGnssConfig& cfg)
    : clock_(clock), route_(route), cfg_(cfg), rng_(cfg.seed) {}

bool MockGnss::begin(uint32_t) {
  started_ = true;
  nextEpoch_ = 0;
  len_ = pos_ = 0;
  return true;
}

RoutePose MockGnss::truthAt(uint32_t s) const { return route_.poseAt(cfg_.speedMps * s); }

bool MockGnss::hasFixAt(uint32_t s) const {
  if (s < cfg_.coldStartS) return false;
  if (cfg_.outagePeriodS == 0 || cfg_.outageDurationS == 0) return true;
  uint32_t phase = s % cfg_.outagePeriodS;
  // Outage sits in the middle of each period, so the first fix is not lost at once.
  uint32_t start = cfg_.outagePeriodS / 2;
  return !(phase >= start && phase < start + cfg_.outageDurationS);
}

void MockGnss::generateEpoch(uint32_t s) {
  nmea::Epoch e;
  int64_t utc = cfg_.startUtcMs + static_cast<int64_t>(s) * 1000;
  e.timeValid = s >= cfg_.timeAfterS;
  e.dateValid = e.timeValid;
  e.timeOfDayMs = static_cast<uint32_t>(utc % kMsPerDay);
  e.epochDays = utc / kMsPerDay;

  // Slowly varying constellation quality
  float wave = std::sin(static_cast<float>(s) * 0.021f);
  int sats = 9 + static_cast<int>(std::lround(2.0f * wave)) + static_cast<int>(rng_.next() % 2);

  // Bias random walk (multipath-like), bounded
  biasE_ += cfg_.biasStepM * rng_.gauss();
  biasN_ += cfg_.biasStepM * rng_.gauss();
  biasE_ = std::fmax(-cfg_.biasLimitM, std::fmin(cfg_.biasLimitM, biasE_));
  biasN_ = std::fmax(-cfg_.biasLimitM, std::fmin(cfg_.biasLimitM, biasN_));

  if (hasFixAt(s)) {
    RoutePose truth = truthAt(s);
    double dE = biasE_ + cfg_.noiseSigmaM * rng_.gauss();
    double dN = biasN_ + cfg_.noiseSigmaM * rng_.gauss();
    double lat0, lon0, lat1, lon1;
    route_.toLatLon(0, 0, lat0, lon0);
    route_.toLatLon(dE, dN, lat1, lon1);
    e.fixQuality = 1;
    e.lat = truth.lat + (lat1 - lat0);
    e.lon = truth.lon + (lon1 - lon0);
    e.alt = cfg_.altitudeM + 2.0f * rng_.gauss();
    e.geoidSep = 36.2f;
    e.satellites = sats;
    e.hdop = 0.9f + 0.4f * (1.0f - wave) + 0.1f * std::fabs(rng_.gauss());
    e.speedKmh = static_cast<float>(cfg_.speedMps * 3.6) + 0.3f * rng_.gauss();
    if (e.speedKmh < 0) e.speedKmh = 0;
    e.courseDeg = static_cast<float>(std::fmod(truth.headingDeg + 3.0 * rng_.gauss() + 360.0, 360.0));
  } else {
    e.satellites = s < cfg_.coldStartS ? static_cast<int>(s / 3) : 2 + static_cast<int>(rng_.next() % 2);
    e.hdop = 99.99f;
  }

  // Keep unread bytes (as a UART FIFO would), then append the new epoch.
  if (pos_ > 0) {
    std::memmove(buf_, buf_ + pos_, len_ - pos_);
    len_ -= pos_;
    pos_ = 0;
  }
  size_t n = nmea::writeEpoch(e, buf_ + len_, sizeof(buf_) - len_);
  len_ += n;  // n == 0: buffer overrun, epoch dropped (like a full UART FIFO)
}

size_t MockGnss::read(char* out, size_t maxLen) {
  if (!started_ || cfg_.silent) return 0;
  const uint64_t lagUs = static_cast<uint64_t>(cfg_.outputLagMs) * 1000ULL;
  const uint64_t now = clock_.nowUs();
  if (now < lagUs) return 0;
  int64_t nowS = static_cast<int64_t>((now - lagUs) / 1000000ULL);
  uint32_t produced = 0;
  while (nextEpoch_ <= nowS && produced < kMaxEpochsPerRead) {
    generateEpoch(static_cast<uint32_t>(nextEpoch_));
    ++nextEpoch_;
    ++produced;
  }
  if (nextEpoch_ <= nowS) nextEpoch_ = nowS + 1;  // skip epochs we could not deliver

  size_t n = len_ - pos_;
  if (n > maxLen) n = maxLen;
  std::memcpy(out, buf_ + pos_, n);
  pos_ += n;
  return n;
}

}  // namespace hal
