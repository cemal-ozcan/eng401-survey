#pragma once

// Fills a Record from the latest GNSS solution and sensor samples. Pure logic
// (no Arduino), shared by the app tasks and native tests.

#include <cstdint>

#include "hal.h"
#include "nmea_parser.h"
#include "record.h"

namespace record {

// GNSS part: position (only with a valid fix), quality, UTM, solution age.
// gnssUpdateUs = clock time when the last GGA/RMC was decoded (0 = never).
void fillGnss(Record& r, const nmea::GnssData& g, uint64_t gnssUpdateUs, uint64_t nowUs);

// Sensor parts: pass nullptr when the latest read failed; the fields stay
// invalid (NAN / 0) and the matching flag stays clear.
void fillImu(Record& r, const hal::ImuSample* s);
void fillEnv(Record& r, const hal::EnvSample* s);
void fillRange(Record& r, const hal::RangeSample* s);

}  // namespace record
