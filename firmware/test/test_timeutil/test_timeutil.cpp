#include <unity.h>

#include "civil_time.h"

void setUp() {}
void tearDown() {}

void test_epoch_and_known_dates() {
  TEST_ASSERT_EQUAL_INT64(0, timeutil::daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT64(10957, timeutil::daysFromCivil(2000, 1, 1));
  TEST_ASSERT_EQUAL_INT64(20759, timeutil::daysFromCivil(2026, 11, 2));
  TEST_ASSERT_EQUAL_INT64(-1, timeutil::daysFromCivil(1969, 12, 31));
}

void test_round_trip_including_leap_day() {
  for (int64_t d = timeutil::daysFromCivil(1999, 1, 1); d < timeutil::daysFromCivil(2101, 1, 1); ++d) {
    timeutil::CivilDate c = timeutil::civilFromDays(d);
    TEST_ASSERT_EQUAL_INT64(d, timeutil::daysFromCivil(c.year, c.month, c.day));
  }
  timeutil::CivilDate leap = timeutil::civilFromDays(timeutil::daysFromCivil(2028, 2, 29));
  TEST_ASSERT_EQUAL_UINT(2, leap.month);
  TEST_ASSERT_EQUAL_UINT(29, leap.day);
}

void test_iso_format() {
  char buf[timeutil::kIsoLen];
  int64_t ms = timeutil::daysFromCivil(2026, 11, 2) * 86400000LL + ((10 * 60 + 15) * 60 + 42) * 1000LL + 7;
  TEST_ASSERT_TRUE(timeutil::formatIso8601(ms, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("2026-11-02T10:15:42.007Z", buf);
  TEST_ASSERT_FALSE(timeutil::formatIso8601(-1, buf, sizeof(buf)));
  TEST_ASSERT_FALSE(timeutil::formatIso8601(ms, buf, 10));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_epoch_and_known_dates);
  RUN_TEST(test_round_trip_including_leap_day);
  RUN_TEST(test_iso_format);
  return UNITY_END();
}
