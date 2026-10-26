#include <unity.h>
#include <cstring>

#include "chip_id.h"
#include "report.h"

void setUp() {}
void tearDown() {}

void test_address_names() {
  TEST_ASSERT_EQUAL_STRING("MPU6050 (AD0=GND)", diag::nameForAddress(0x68));
  TEST_ASSERT_EQUAL_STRING("VL53L0X / VL53L1X", diag::nameForAddress(0x29));
  TEST_ASSERT_EQUAL_STRING("unknown device", diag::nameForAddress(0x50));
}

void test_bosch_detects_bmp280_impostor() {
  TEST_ASSERT_TRUE(diag::isBme280(0x60));
  TEST_ASSERT_FALSE(diag::isBme280(0x58));
  TEST_ASSERT_EQUAL_STRING("BMP280 (no humidity!)", diag::boschName(0x58));
}

void test_mpu_clone_detection() {
  TEST_ASSERT_TRUE(diag::isMpu6050(0x68));
  TEST_ASSERT_FALSE(diag::isMpu6050(0x70));
  TEST_ASSERT_EQUAL_STRING("MPU6500", diag::mpuName(0x70));
}

void test_tof_names() {
  TEST_ASSERT_EQUAL_STRING("VL53L0X", diag::tofName(0xEE, 0));
  TEST_ASSERT_EQUAL_STRING("VL53L1X", diag::tofName(0x00, 0xEACC));
  TEST_ASSERT_EQUAL_STRING("unknown ToF chip", diag::tofName(0, 0));
}

void test_report_counts_and_summary() {
  diag::Report r;
  r.set("I2C", diag::Status::Ok, "3 devices");
  r.set("BME280", diag::Status::Fail, "not found");
  r.set("GNSS", diag::Status::Warn, "data but no fix");
  r.set("BME280", diag::Status::Ok, "replaced");  // same name -> replace, not append
  TEST_ASSERT_EQUAL_UINT(3, r.count());
  TEST_ASSERT_EQUAL_UINT(2, r.countWith(diag::Status::Ok));
  char line[80];
  r.summaryLine(line, sizeof(line));
  TEST_ASSERT_EQUAL_STRING("=== REPORT: 2/3 OK, 1 WARN, 0 FAIL ===", line);
}

void test_report_truncates_long_detail() {
  diag::Report r;
  char longText[300];
  std::memset(longText, 'x', sizeof(longText) - 1);
  longText[sizeof(longText) - 1] = '\0';
  r.set("SD", diag::Status::Ok, longText);
  TEST_ASSERT_EQUAL_UINT(sizeof(r.at(0).detail) - 1, std::strlen(r.at(0).detail));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_address_names);
  RUN_TEST(test_bosch_detects_bmp280_impostor);
  RUN_TEST(test_mpu_clone_detection);
  RUN_TEST(test_tof_names);
  RUN_TEST(test_report_counts_and_summary);
  RUN_TEST(test_report_truncates_long_detail);
  return UNITY_END();
}
