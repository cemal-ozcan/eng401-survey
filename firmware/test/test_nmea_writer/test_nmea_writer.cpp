#include <unity.h>

#include <cstring>

#include "nmea_parser.h"
#include "nmea_writer.h"

void setUp() {}
void tearDown() {}

static nmea::Epoch fixEpoch() {
  nmea::Epoch e;
  e.timeValid = e.dateValid = true;
  e.timeOfDayMs = 36942000;  // 10:15:42
  e.epochDays = 20759;       // 2026-11-02
  e.fixQuality = 1;
  e.lat = 39.92077;
  e.lon = 32.85411;
  e.alt = 938.4f;
  e.geoidSep = 36.2f;
  e.satellites = 9;
  e.hdop = 0.92f;
  e.speedKmh = 3.6f;
  e.courseDeg = 87.2f;
  return e;
}

static nmea::Parser parseAll(const char* text) {
  nmea::Parser p;
  for (const char* c = text; *c; ++c) p.feed(*c);
  return p;
}

void test_finish_sentence_checksum() {
  char s[100] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,";
  TEST_ASSERT_TRUE(nmea::finishSentence(s, sizeof(s)) > 0);
  TEST_ASSERT_EQUAL_STRING("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n", s);
}

void test_round_trip_fix() {
  char buf[512];
  TEST_ASSERT_TRUE(nmea::writeEpoch(fixEpoch(), buf, sizeof(buf)) > 0);
  nmea::Parser p = parseAll(buf);
  TEST_ASSERT_EQUAL_UINT32(0, p.stats().checksumErrors);
  TEST_ASSERT_EQUAL_UINT32(3, p.stats().sentences);
  const nmea::GnssData& d = p.data();
  TEST_ASSERT_TRUE(d.posValid);
  // 5 decimal minutes = 1.7e-7 deg resolution (~2 cm)
  TEST_ASSERT_DOUBLE_WITHIN(2e-7, 39.92077, d.lat);
  TEST_ASSERT_DOUBLE_WITHIN(2e-7, 32.85411, d.lon);
  TEST_ASSERT_EQUAL_INT(9, d.satellites);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.6f, d.speedKmh);
  TEST_ASSERT_EQUAL_INT64(20759LL * 86400000LL + 36942000LL, d.utcMs());
}

void test_round_trip_no_fix_and_southwest() {
  nmea::Epoch e = fixEpoch();
  e.fixQuality = 0;
  char buf[512];
  nmea::writeEpoch(e, buf, sizeof(buf));
  nmea::Parser p = parseAll(buf);
  TEST_ASSERT_FALSE(p.data().posValid);
  TEST_ASSERT_TRUE(p.data().timeValid);

  e = fixEpoch();
  e.lat = -33.8688;
  e.lon = -70.6693;
  nmea::writeEpoch(e, buf, sizeof(buf));
  p = parseAll(buf);
  TEST_ASSERT_DOUBLE_WITHIN(2e-7, -33.8688, p.data().lat);
  TEST_ASSERT_DOUBLE_WITHIN(2e-7, -70.6693, p.data().lon);
}

void test_minutes_rounding_edge() {
  nmea::Epoch e = fixEpoch();
  e.lat = 39.9999999999;  // would print as 39 deg 60.00000 min without the guard
  char buf[512];
  nmea::writeEpoch(e, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(std::strstr(buf, "4000.00000,N"));
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 40.0, parseAll(buf).data().lat);
}

void test_too_small_buffer() {
  char buf[60];
  TEST_ASSERT_EQUAL_UINT(0, nmea::writeEpoch(fixEpoch(), buf, sizeof(buf)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_finish_sentence_checksum);
  RUN_TEST(test_round_trip_fix);
  RUN_TEST(test_round_trip_no_fix_and_southwest);
  RUN_TEST(test_minutes_rounding_edge);
  RUN_TEST(test_too_small_buffer);
  return UNITY_END();
}
