#pragma once

// Builds NMEA sentences (talker "GN", like a NEO-M8N). Used by the mock GNSS
// so simulated data goes through exactly the same parser as real data.

#include <cstddef>
#include <cstdint>

namespace nmea {

struct Epoch {
  bool timeValid = false;   // receiver knows the time
  uint32_t timeOfDayMs = 0;
  bool dateValid = false;
  int64_t epochDays = 0;

  int fixQuality = 0;       // 0 = no fix
  double lat = 0.0, lon = 0.0;
  float alt = 0.0f;
  float geoidSep = 0.0f;
  int satellites = 0;
  float hdop = 99.99f;
  float speedKmh = 0.0f;
  float courseDeg = 0.0f;
};

// Appends "*HH\r\n" to a sentence body that starts with '$'.
// Returns total length, 0 if it does not fit.
size_t finishSentence(char* sentence, size_t size);

// Writes RMC, VTG and GGA (in that order, as u-blox does) with CRLF endings.
// Returns bytes written, 0 if the buffer is too small.
size_t writeEpoch(const Epoch& e, char* out, size_t size);

}  // namespace nmea
