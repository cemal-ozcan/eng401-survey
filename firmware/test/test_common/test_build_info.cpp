#include <unity.h>
#include <cstring>

#include "build_info.h"

void setUp() {}
void tearDown() {}

void test_version_is_not_empty() { TEST_ASSERT_TRUE(std::strlen(build_info::version()) > 0); }

void test_mode_name_matches_flag() {
  TEST_ASSERT_EQUAL_STRING(build_info::isMock() ? "MOCK" : "REAL", build_info::modeName());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_version_is_not_empty);
  RUN_TEST(test_mode_name_matches_flag);
  return UNITY_END();
}
