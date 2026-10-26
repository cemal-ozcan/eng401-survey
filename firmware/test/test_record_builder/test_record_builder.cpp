#include <unity.h>

#include <cmath>

#include "nmea_parser.h"
#include "record_builder.h"

void setUp() {}
void tearDown() {}

static nmea::GnssData fixData() {
  nmea::Parser p;
  p.processSentence("$GNRMC,101542.00,A,3955.24620,N,03251.24660,E,1.944,87.20,021126,,,A*4B");
  p.processSentence("$GNGGA,101542.00,3955.24620,N,03251.24660,E,1,09,0.92,938.4,M,36.2,M,,*49");
  return p.data();
}

void test_fix_fills_position_utm_and_flag() {
  record::Record r;
  record::fillGnss(r, fixData(), 1000000, 1180000);
  TEST_ASSERT_TRUE(r.flags & record::kFlagGnssFix);
  TEST_ASSERT_EQUAL_UINT8(36, r.utmZone);
  TEST_ASSERT_DOUBLE_WITHIN(0.001, 487532.490, r.easting);
  TEST_ASSERT_DOUBLE_WITHIN(0.001, 4418973.710, r.northing);
  TEST_ASSERT_EQUAL_UINT32(180, r.gnssAgeMs);
  TEST_ASSERT_EQUAL_UINT8(9, r.sats);
}

void test_no_fix_keeps_position_invalid_but_quality() {
  nmea::Parser p;
  p.processSentence("$GNGGA,101543.00,,,,,0,03,25.50,,,,,,*7B");
  record::Record r;
  record::fillGnss(r, p.data(), 0, 5000000);
  TEST_ASSERT_FALSE(r.flags & record::kFlagGnssFix);
  TEST_ASSERT_TRUE(std::isnan(r.lat));
  TEST_ASSERT_EQUAL_UINT8(0, r.utmZone);
  TEST_ASSERT_EQUAL_UINT8(3, r.sats);
  TEST_ASSERT_EQUAL_UINT32(record::kAgeUnknown, r.gnssAgeMs);
}

void test_sensor_fill_and_failures() {
  record::Record r;
  hal::ImuSample imu{0.1f, 0.2f, 9.8f, 1, 2, 3, 25};
  hal::EnvSample env{18.4f, 912.3f, 41.0f};
  hal::RangeSample noTarget{0, false};
  record::fillImu(r, &imu);
  record::fillEnv(r, nullptr);  // BME280 read failed
  record::fillRange(r, &noTarget);
  TEST_ASSERT_EQUAL_UINT16(record::kFlagImuOk, r.flags);
  TEST_ASSERT_FLOAT_WITHIN(1e-6, 9.8f, r.az);
  TEST_ASSERT_TRUE(std::isnan(r.temp));
  TEST_ASSERT_EQUAL_UINT16(0, r.dist);
  record::fillEnv(r, &env);
  TEST_ASSERT_TRUE(r.flags & record::kFlagEnvOk);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fix_fills_position_utm_and_flag);
  RUN_TEST(test_no_fix_keeps_position_invalid_but_quality);
  RUN_TEST(test_sensor_fill_and_failures);
  return UNITY_END();
}
