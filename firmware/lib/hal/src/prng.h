#pragma once

// Small deterministic PRNG (xorshift32) for mocks: same seed -> same data,
// so failures seen in simulation can be reproduced exactly.

#include <cmath>
#include <cstdint>

namespace hal {

class Prng {
 public:
  explicit Prng(uint32_t seed) : state_(seed != 0 ? seed : 0x9E3779B9u) {}

  uint32_t next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
  }

  // Uniform in [0, 1).
  float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

  bool chance(float probability) { return uniform() < probability; }

  // Standard normal (Box-Muller).
  float gauss() {
    if (hasSpare_) {
      hasSpare_ = false;
      return spare_;
    }
    float u1 = uniform(), u2 = uniform();
    if (u1 < 1e-7f) u1 = 1e-7f;
    float r = std::sqrt(-2.0f * std::log(u1));
    spare_ = r * std::sin(6.2831853f * u2);
    hasSpare_ = true;
    return r * std::cos(6.2831853f * u2);
  }

 private:
  uint32_t state_;
  float spare_ = 0.0f;
  bool hasSpare_ = false;
};

}  // namespace hal
