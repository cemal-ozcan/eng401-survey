#include <unity.h>
#include <cstring>

#include "nmea_basic.h"

void setUp() {}
void tearDown() {}

// Real-world style sentences (checksums computed for these exact strings).
static const char* kGgaFix = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
static const char* kGgaNoFix = "$GNGGA,,,,,,0,00,99.99,,,,,,*56";

static void feed(nmea::LineBuffer& lb, const char* text, int& lines, char* last) {
  for (const char* p = text; *p; ++p) {
    if (lb.push(*p)) {
      ++lines;
      std::strcpy(last, lb.line());
    }
  }
}

void test_checksum_valid() { TEST_ASSERT_TRUE(nmea::checksumOk(kGgaFix)); }

void test_checksum_detects_corruption() {
  char bad[100];
  std::strcpy(bad, kGgaFix);
  bad[10] = '9';
  TEST_ASSERT_FALSE(nmea::checksumOk(bad));
}

void test_checksum_rejects_missing_star_and_null() {
  TEST_ASSERT_FALSE(nmea::checksumOk("$GPGGA,1,2,3"));
  TEST_ASSERT_FALSE(nmea::checksumOk(nullptr));
  TEST_ASSERT_FALSE(nmea::checksumOk("GPGGA*47"));
}

void test_sentence_type_any_talker() {
  char t[4];
  TEST_ASSERT_TRUE(nmea::sentenceType(kGgaNoFix, t));
  TEST_ASSERT_EQUAL_STRING("GGA", t);
  TEST_ASSERT_TRUE(nmea::sentenceType("$GLGSV,1,1,00*65", t));
  TEST_ASSERT_EQUAL_STRING("GSV", t);
}

void test_gga_quick_with_fix() {
  nmea::GgaQuick g;
  TEST_ASSERT_TRUE(nmea::parseGgaQuick(kGgaFix, g));
  TEST_ASSERT_EQUAL_INT(1, g.fixQuality);
  TEST_ASSERT_EQUAL_INT(8, g.satellites);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 0.9f, g.hdop);
}

void test_gga_quick_without_fix() {
  nmea::GgaQuick g;
  TEST_ASSERT_TRUE(nmea::parseGgaQuick(kGgaNoFix, g));
  TEST_ASSERT_EQUAL_INT(0, g.fixQuality);
  TEST_ASSERT_EQUAL_INT(0, g.satellites);
}

void test_gga_quick_rejects_other_sentences() {
  nmea::GgaQuick g;
  TEST_ASSERT_FALSE(nmea::parseGgaQuick("$GLGSV,1,1,00*65", g));
}

void test_line_buffer_splits_stream() {
  nmea::LineBuffer lb;
  int lines = 0;
  char last[100] = {0};
  // Garbage before the first '$' (typical when connecting mid-sentence).
  feed(lb, "8,0.9*1F\r\n", lines, last);
  TEST_ASSERT_EQUAL_INT(0, lines);
  feed(lb, kGgaFix, lines, last);
  feed(lb, "\r\n", lines, last);
  feed(lb, kGgaNoFix, lines, last);
  feed(lb, "\r\n", lines, last);
  TEST_ASSERT_EQUAL_INT(2, lines);
  TEST_ASSERT_EQUAL_STRING(kGgaNoFix, last);
}

void test_line_buffer_drops_overlong_lines() {
  nmea::LineBuffer lb;
  int lines = 0;
  char last[100] = {0};
  feed(lb, "$", lines, last);
  for (int i = 0; i < 200; ++i) lb.push('A');
  feed(lb, "\r\n", lines, last);
  TEST_ASSERT_EQUAL_INT(0, lines);
  TEST_ASSERT_EQUAL_UINT32(1, lb.overflows());
  feed(lb, kGgaFix, lines, last);
  feed(lb, "\n", lines, last);
  TEST_ASSERT_EQUAL_INT(1, lines);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_checksum_valid);
  RUN_TEST(test_checksum_detects_corruption);
  RUN_TEST(test_checksum_rejects_missing_star_and_null);
  RUN_TEST(test_sentence_type_any_talker);
  RUN_TEST(test_gga_quick_with_fix);
  RUN_TEST(test_gga_quick_without_fix);
  RUN_TEST(test_gga_quick_rejects_other_sentences);
  RUN_TEST(test_line_buffer_splits_stream);
  RUN_TEST(test_line_buffer_drops_overlong_lines);
  return UNITY_END();
}
