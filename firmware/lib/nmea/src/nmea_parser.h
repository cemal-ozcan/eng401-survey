#pragma once

// NMEA 0183 parser for the survey firmware. Model-independent: accepts any
// talker (GP, GN, GL, GA, BD) and the standard GGA / RMC / VTG sentences that
// u-blox NEO-6M and NEO-M8N output by default.
//
// The parser has no clock: feed() reports which sentence was decoded and the
// caller timestamps it (esp_timer on the device, file time in replay).

#include <cmath>
#include <cstdint>

#include "nmea_basic.h"

namespace nmea {

enum class Sentence : uint8_t { None, GGA, RMC, VTG, Other, ChecksumError };

struct GnssData {
  // Position (valid when posValid)
  bool posValid = false;
  double lat = NAN;  // deg, + north
  double lon = NAN;  // deg, + east
  float alt = NAN;   // m above MSL (GGA)

  // Quality (GGA)
  int fixQuality = 0;  // 0 none, 1 GPS, 2 DGPS, 4/5 RTK, 6 dead reckoning
  int satellites = 0;
  float hdop = NAN;

  // Motion (RMC / VTG)
  float speedKmh = NAN;
  float courseDeg = NAN;

  // Time: time of day from GGA/RMC, date from RMC only
  bool timeValid = false;      // timeOfDayMs is set
  bool dateValid = false;      // epochDays is set
  uint32_t timeOfDayMs = 0;    // ms since 00:00:00 UTC
  int64_t epochDays = 0;       // days since 1970-01-01

  // Unix epoch ms of the last decoded fix time; -1 until date and time known.
  int64_t utcMs() const;
};

struct ParserStats {
  uint32_t sentences = 0;       // checksum-valid sentences
  uint32_t checksumErrors = 0;
  uint32_t gga = 0, rmc = 0, vtg = 0;
};

class Parser {
 public:
  // Feed one byte from the UART. Returns the sentence type when a line was
  // completed, Sentence::None otherwise.
  Sentence feed(char c);

  // Decode one complete sentence ("$....*HH", no line ending).
  Sentence processSentence(const char* sentence);

  const GnssData& data() const { return data_; }
  const ParserStats& stats() const { return stats_; }

 private:
  bool parseGga(char** f, int n);
  bool parseRmc(char** f, int n);
  bool parseVtg(char** f, int n);

  LineBuffer line_;
  GnssData data_;
  ParserStats stats_;
};

// Helpers exposed for unit tests.
// "4807.038","N" -> 48.1173 ; returns NAN for empty/malformed input.
double parseCoordinate(const char* value, const char* hemisphere);
// "123519.50" -> ms of day; returns false if malformed.
bool parseTimeOfDay(const char* value, uint32_t& msOfDay);
// "230394" (ddmmyy) -> days since epoch; years 80-99 -> 19xx, else 20xx.
bool parseDate(const char* value, int64_t& epochDays);

}  // namespace nmea
