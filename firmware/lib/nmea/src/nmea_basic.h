#pragma once

// Minimal, platform-independent NMEA 0183 helpers used by the diagnostic
// firmware. The full parser (RMC/VTG/GSA, UTC time) is added in step 2.

#include <cstddef>
#include <cstdint>

namespace nmea {

constexpr size_t kMaxSentenceLen = 96;  // NMEA allows 82; leave headroom

// Collects bytes from a UART into complete sentences ("$....*HH").
// push() returns true when a full line is available via line().
class LineBuffer {
 public:
  bool push(char c);
  const char* line() const { return buf_; }
  // Number of lines dropped because they exceeded kMaxSentenceLen.
  uint32_t overflows() const { return overflows_; }

 private:
  char buf_[kMaxSentenceLen + 1] = {0};
  size_t len_ = 0;
  bool inSentence_ = false;
  bool overflowed_ = false;
  uint32_t overflows_ = 0;
};

// True if the sentence is "$<body>*HH" and HH equals the XOR of <body>.
bool checksumOk(const char* sentence);

// Copies the 3-letter sentence type ("GGA", "RMC", ...) into out[4].
// Works for any 2-letter talker (GP, GN, GL, GA, BD). Returns false if malformed.
bool sentenceType(const char* sentence, char out[4]);

struct GgaQuick {
  int fixQuality = 0;   // 0 = invalid, 1 = GPS fix, 2 = DGPS, ...
  int satellites = 0;
  float hdop = -1.0f;   // -1 when the field is empty
};

// Extracts fix quality, satellites used and HDOP from a GGA sentence.
// Returns false if the sentence is not a checksum-valid GGA.
bool parseGgaQuick(const char* sentence, GgaQuick& out);

}  // namespace nmea
