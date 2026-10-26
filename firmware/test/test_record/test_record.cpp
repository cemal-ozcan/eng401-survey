// Record encoders must reproduce the golden files in shared/golden/ byte for
// byte; the PC codec tests decode the very same files.

#include <unity.h>

#include <cstdio>
#include <cstring>

#include "utm.h"
#include "record.h"

void setUp() {}
void tearDown() {}

// Full record: GNSS fix in Ankara, all sensors OK.
static record::Record fullRecord() {
  record::Record r;
  r.seq = 42;
  r.tUs = 43012345;
  r.utcMs = 20759LL * 86400000LL + 36942000LL;  // 2026-11-02T10:15:42.000Z
  r.fix = 1;
  r.sats = 9;
  r.hdop = 0.92f;
  r.lat = 39.92077;
  r.lon = 32.85411;
  r.alt = 938.4f;
  r.spd = 3.6f;
  r.cog = 87.2f;
  r.gnssAgeMs = 180;
  geo::Utm u{};
  geo::latLonToUtm(r.lat, r.lon, u);
  r.utmZone = u.zone;
  r.utmHemi = u.hemisphere;
  r.easting = u.easting;
  r.northing = u.northing;
  r.ax = 0.12f; r.ay = -0.05f; r.az = 9.79f;
  r.gx = 0.3f; r.gy = -0.1f; r.gz = 0.0f;
  r.temp = 18.4f;
  r.pres = 912.3f;
  r.hum = 41.0f;
  r.dist = 1532;
  r.flags = 0x7F;
  r.missed = 0;
  r.mark = 0;
  return r;
}

// Degraded record: no fix, no UTC, BME280 failed, ToF out of range.
static record::Record degradedRecord() {
  record::Record r;
  r.seq = 43;
  r.tUs = 44012001;
  r.sats = 3;
  r.hdop = 25.5f;
  r.ax = 0.11f; r.ay = -0.04f; r.az = 9.81f;
  r.gx = 0.2f; r.gy = 0.0f; r.gz = -0.1f;
  r.flags = record::kFlagImuOk | record::kFlagSdOk | record::kFlagWifi;
  r.missed = 2;
  r.mark = 1;
  return r;
}

// Reads line `index` (0-based) of a golden file without its line ending.
static bool goldenLine(const char* name, int index, char* out, size_t size) {
  char path[256];
  std::snprintf(path, sizeof(path), "%s/%s", GOLDEN_DIR, name);
  FILE* f = std::fopen(path, "r");
  if (!f) return false;
  bool found = false;
  for (int i = 0; i <= index && std::fgets(out, static_cast<int>(size), f); ++i) found = (i == index);
  std::fclose(f);
  if (found) out[std::strcspn(out, "\r\n")] = '\0';
  return found;
}

static void checkAgainstGolden(const record::Record& r, int row) {
  char buf[record::kMaxJsonLen], expect[record::kMaxJsonLen];

  TEST_ASSERT_TRUE(record::toCsv(r, buf, sizeof(buf)) > 0);
  TEST_ASSERT_TRUE(goldenLine("record_example.csv", row + 1, expect, sizeof(expect)));
  TEST_ASSERT_EQUAL_STRING(expect, buf);

  TEST_ASSERT_TRUE(record::toJson(r, buf, sizeof(buf)) > 0);
  TEST_ASSERT_TRUE(goldenLine("record_example.jsonl", row, expect, sizeof(expect)));
  TEST_ASSERT_EQUAL_STRING(expect, buf);
}

void test_header_matches_golden() {
  char expect[512];
  TEST_ASSERT_TRUE(goldenLine("record_example.csv", 0, expect, sizeof(expect)));
  TEST_ASSERT_EQUAL_STRING(expect, record::csvHeader());
}

void test_full_record_golden() { checkAgainstGolden(fullRecord(), 0); }

void test_degraded_record_golden() { checkAgainstGolden(degradedRecord(), 1); }

void test_buffer_too_small_returns_zero() {
  char small[40];
  TEST_ASSERT_EQUAL_UINT(0, record::toCsv(fullRecord(), small, sizeof(small)));
  TEST_ASSERT_EQUAL_UINT(0, record::toJson(fullRecord(), small, sizeof(small)));
}

void test_worst_case_fits_max_len() {
  record::Record r = fullRecord();
  r.seq = 0xFFFFFFFFu;
  r.tUs = 0xFFFFFFFFFFFFFFFFull;
  r.lat = -89.12345678;
  r.lon = -179.12345678;
  r.alt = -12345.6f; r.spd = 1234.56f; r.cog = 359.9f;
  r.ax = r.ay = r.az = -156.789f;
  r.gx = r.gy = r.gz = -2000.0f;
  r.pres = 1100.0f; r.temp = -40.0f;
  r.gnssAgeMs = 0xFFFFFFFEu;
  r.missed = 0xFFFFFFFFu;
  r.dist = r.flags = r.mark = 0xFFFF;
  char buf[record::kMaxJsonLen];
  size_t csv = record::toCsv(r, buf, record::kMaxCsvLen);
  size_t json = record::toJson(r, buf, record::kMaxJsonLen);
  std::printf("worst case: csv %u chars, json %u chars\n", static_cast<unsigned>(csv),
              static_cast<unsigned>(json));
  TEST_ASSERT_TRUE(csv > 0);
  TEST_ASSERT_TRUE(json > 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_header_matches_golden);
  RUN_TEST(test_full_record_golden);
  RUN_TEST(test_degraded_record_golden);
  RUN_TEST(test_buffer_too_small_returns_zero);
  RUN_TEST(test_worst_case_fits_max_len);
  return UNITY_END();
}
