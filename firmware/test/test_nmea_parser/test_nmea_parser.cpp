#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "nmea_parser.h"

void setUp() {}
void tearDown() {}

// Sentences shaped like NEO-M8N output (talker GN), position in Ankara.
static const char* kRmc = "$GNRMC,101542.00,A,3955.24620,N,03251.24660,E,1.944,87.20,021126,,,A*4B";
static const char* kGga = "$GNGGA,101542.00,3955.24620,N,03251.24660,E,1,09,0.92,938.4,M,36.2,M,,*49";
static const char* kVtg = "$GNVTG,87.20,T,,M,1.944,N,3.600,K,A*13";
static const char* kRmcLost = "$GNRMC,101543.00,V,,,,,,,021126,,,N*67";
static const char* kGgaLost = "$GNGGA,101543.00,,,,,0,03,25.50,,,,,,*7B";
static const char* kRmcSouth = "$GPRMC,235959.999,A,3352.1280,S,15112.5580,E,0.0,0.0,311299,,*15";

void test_coordinate_conversion() {
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 39.92077, nmea::parseCoordinate("3955.24620", "N"));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, -151.20930, nmea::parseCoordinate("15112.5580", "W"));
  TEST_ASSERT_TRUE(std::isnan(nmea::parseCoordinate("", "N")));
  TEST_ASSERT_TRUE(std::isnan(nmea::parseCoordinate("3975.0", "N")));  // 75 minutes
  TEST_ASSERT_TRUE(std::isnan(nmea::parseCoordinate("3955.2", "X")));
}

void test_time_and_date_parsing() {
  uint32_t ms;
  TEST_ASSERT_TRUE(nmea::parseTimeOfDay("101542.50", ms));
  TEST_ASSERT_EQUAL_UINT32((10 * 3600 + 15 * 60 + 42) * 1000 + 500, ms);
  TEST_ASSERT_FALSE(nmea::parseTimeOfDay("256000", ms));
  TEST_ASSERT_FALSE(nmea::parseTimeOfDay("", ms));
  int64_t days;
  TEST_ASSERT_TRUE(nmea::parseDate("021126", days));
  TEST_ASSERT_EQUAL_INT64(20759, days);  // 2026-11-02
  TEST_ASSERT_FALSE(nmea::parseDate("321126", days));
}

void test_full_fix_sequence() {
  nmea::Parser p;
  TEST_ASSERT_EQUAL(nmea::Sentence::RMC, p.processSentence(kRmc));
  TEST_ASSERT_EQUAL(nmea::Sentence::GGA, p.processSentence(kGga));
  TEST_ASSERT_EQUAL(nmea::Sentence::VTG, p.processSentence(kVtg));
  const nmea::GnssData& d = p.data();
  TEST_ASSERT_TRUE(d.posValid);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 39.92077, d.lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 32.85411, d.lon);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 938.4f, d.alt);
  TEST_ASSERT_EQUAL_INT(1, d.fixQuality);
  TEST_ASSERT_EQUAL_INT(9, d.satellites);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 0.92f, d.hdop);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 3.6f, d.speedKmh);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 87.2f, d.courseDeg);
  // 2026-11-02T10:15:42.000Z
  TEST_ASSERT_EQUAL_INT64(20759LL * 86400000LL + 36942000LL, d.utcMs());
}

void test_fix_loss_invalidates_position() {
  nmea::Parser p;
  p.processSentence(kRmc);
  p.processSentence(kGga);
  p.processSentence(kRmcLost);
  TEST_ASSERT_FALSE(p.data().posValid);
  p.processSentence(kGgaLost);
  TEST_ASSERT_FALSE(p.data().posValid);
  TEST_ASSERT_EQUAL_INT(0, p.data().fixQuality);
  TEST_ASSERT_EQUAL_INT(3, p.data().satellites);
  TEST_ASSERT_FLOAT_WITHIN(1e-3, 25.5f, p.data().hdop);
  // Time keeps running without a fix (receiver clock)
  TEST_ASSERT_EQUAL_UINT32(36943000u, p.data().timeOfDayMs);
}

void test_utc_unknown_until_rmc_date() {
  nmea::Parser p;
  p.processSentence(kGga);
  TEST_ASSERT_TRUE(p.data().timeValid);
  TEST_ASSERT_EQUAL_INT64(-1, p.data().utcMs());
}

void test_southern_hemisphere_and_fraction() {
  nmea::Parser p;
  p.processSentence(kRmcSouth);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, -33.86880, p.data().lat);
  TEST_ASSERT_EQUAL_UINT32(86399999u, p.data().timeOfDayMs);
}

void test_byte_stream_and_checksum_errors() {
  FILE* f = std::fopen(GOLDEN_DIR "/nmea_sample.nmea", "r");
  TEST_ASSERT_NOT_NULL(f);
  nmea::Parser p;
  int c;
  while ((c = std::fgetc(f)) != EOF) p.feed(static_cast<char>(c));
  std::fclose(f);
  // 7 lines, the last one has a wrong checksum on purpose.
  TEST_ASSERT_EQUAL_UINT32(6, p.stats().sentences);
  TEST_ASSERT_EQUAL_UINT32(1, p.stats().checksumErrors);
  TEST_ASSERT_EQUAL_UINT32(2, p.stats().gga);
  TEST_ASSERT_EQUAL_UINT32(2, p.stats().rmc);
  TEST_ASSERT_FALSE(p.data().posValid);  // last valid GGA had no fix
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_coordinate_conversion);
  RUN_TEST(test_time_and_date_parsing);
  RUN_TEST(test_full_fix_sequence);
  RUN_TEST(test_fix_loss_invalidates_position);
  RUN_TEST(test_utc_unknown_until_rmc_date);
  RUN_TEST(test_southern_hemisphere_and_fraction);
  RUN_TEST(test_byte_stream_and_checksum_errors);
  return UNITY_END();
}
