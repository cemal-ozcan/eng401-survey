#include <unity.h>

#include <cstring>
#include <string>

#include "mock_storage.h"
#include "survey_logger.h"

class FakeClock : public hal::Clock {
 public:
  uint64_t us = 0;
  uint64_t nowUs() override { return us; }
};

void setUp() {}
void tearDown() {}

static record::Record rec(uint32_t seq) {
  record::Record r;
  r.seq = seq;
  r.tUs = seq * 1000000ULL;
  // Realistic, fully populated record (~230 bytes of CSV)
  r.utcMs = 1793613600000LL + seq * 1000LL;
  r.fix = 1; r.sats = 9; r.hdop = 0.92f;
  r.lat = 39.89123456; r.lon = 32.78012345; r.alt = 938.4f; r.spd = 4.3f; r.cog = 87.2f;
  r.gnssAgeMs = 180; r.utmZone = 36; r.easting = 481234.567; r.northing = 4415678.901;
  r.ax = 0.1f; r.ay = 0.2f; r.az = 9.8f; r.gx = 0.3f; r.gy = -0.1f; r.gz = 0.02f;
  r.temp = 18.4f; r.pres = 912.3f; r.hum = 41.0f; r.dist = 1532; r.flags = 127;
  return r;
}

static int countLines(const std::string& s) {
  int n = 0;
  for (char c : s) n += c == '\n';
  return n;
}

// seq of every data line, in file order
static std::string seqList(const std::string& s) {
  std::string out;
  size_t pos = s.find('\n') + 1;  // skip header
  while (pos < s.size()) {
    size_t comma = s.find(',', pos);
    out += s.substr(pos, comma - pos) + " ";
    pos = s.find('\n', pos) + 1;
  }
  return out;
}

void test_file_name() {
  char name[32];
  logger::makeFileName("/SURVEY_", 7, name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("/SURVEY_007.CSV", name);
}

void test_new_file_with_header_and_no_overwrite() {
  FakeClock clock;
  hal::MockStorage sd(1 << 20);
  sd.open("/SURVEY_000.CSV");  // left from an earlier survey
  sd.write("old\n", 4);
  sd.close();
  logger::SurveyLogger log(sd, clock);
  TEST_ASSERT_TRUE(log.begin());
  TEST_ASSERT_EQUAL_STRING("/SURVEY_001.CSV", log.fileName());
  TEST_ASSERT_EQUAL_STRING("old\n", sd.contents("/SURVEY_000.CSV").c_str());
  std::string header = std::string(record::csvHeader()) + "\n";
  TEST_ASSERT_EQUAL_STRING(header.c_str(), sd.contents("/SURVEY_001.CSV").c_str());
}

void test_buffers_until_time_flush() {
  FakeClock clock;
  hal::MockStorage sd(1 << 20);
  logger::SurveyLogger log(sd, clock);
  log.begin();
  for (uint32_t i = 0; i < 3; ++i) log.append(rec(i));
  clock.us = 4900000;
  log.tick();
  TEST_ASSERT_EQUAL_INT(1, countLines(sd.contents(log.fileName())));  // header only
  clock.us = 5000000;
  log.tick();
  TEST_ASSERT_EQUAL_INT(4, countLines(sd.contents(log.fileName())));
  TEST_ASSERT_EQUAL_UINT32(3, log.stats().recordsWritten);
}

void test_full_buffer_writes_block() {
  FakeClock clock;
  hal::MockStorage sd(1 << 20);
  logger::SurveyLogger log(sd, clock);
  log.begin();
  for (uint32_t i = 0; i < 100; ++i) log.append(rec(i));  // > 4 KB of CSV
  TEST_ASSERT_TRUE(log.stats().recordsWritten > 0);
  TEST_ASSERT_TRUE(log.stats().recordsWritten < 100);
  log.flushNow();
  TEST_ASSERT_EQUAL_UINT32(100, log.stats().recordsWritten);
  TEST_ASSERT_EQUAL_INT(101, countLines(sd.contents(log.fileName())));
}

void test_rotation_keeps_every_record() {
  FakeClock clock;
  hal::MockStorage sd(1 << 20);
  logger::Config cfg;
  cfg.maxFileBytes = 6000;
  logger::SurveyLogger log(sd, clock, cfg);
  log.begin();
  for (uint32_t i = 0; i < 200; ++i) log.append(rec(i));
  log.flushNow();
  TEST_ASSERT_TRUE(sd.fileCount() >= 3);
  std::string all;
  int dataLines = 0;
  for (int f = 0; f < static_cast<int>(sd.fileCount()); ++f) {
    char name[32];
    logger::makeFileName("/SURVEY_", f, name, sizeof(name));
    const std::string& c = sd.contents(name);
    TEST_ASSERT_EQUAL_INT(0, c.find(record::csvHeader()));  // every file starts with the header
    TEST_ASSERT_TRUE(c.size() <= cfg.maxFileBytes);
    dataLines += countLines(c) - 1;
    all += seqList(c);
  }
  TEST_ASSERT_EQUAL_INT(200, dataLines);
  std::string expect;
  for (int i = 0; i < 200; ++i) expect += std::to_string(i) + " ";
  TEST_ASSERT_EQUAL_STRING(expect.c_str(), all.c_str());
}

void test_card_removed_and_reinserted() {
  FakeClock clock;
  hal::MockStorage sd(1 << 20);
  logger::SurveyLogger log(sd, clock);
  log.begin();
  log.append(rec(0));
  clock.us = 5000000;
  log.tick();  // rec 0 on card

  sd.setPresent(false);
  log.append(rec(1));
  clock.us = 10000000;
  log.tick();  // flush fails -> error
  TEST_ASSERT_FALSE(log.ok());
  TEST_ASSERT_EQUAL_UINT32(1, log.stats().writeErrors);
  log.append(rec(2));  // still buffered in RAM

  sd.setPresent(true);
  clock.us = 12000000;
  log.tick();  // retry not yet due (5 s after the error)
  TEST_ASSERT_FALSE(log.ok());
  clock.us = 15000000;
  log.tick();
  TEST_ASSERT_TRUE(log.ok());
  TEST_ASSERT_EQUAL_STRING("/SURVEY_001.CSV", log.fileName());
  TEST_ASSERT_EQUAL_STRING("1 2 ", seqList(sd.contents("/SURVEY_001.CSV")).c_str());
  TEST_ASSERT_EQUAL_UINT32(0, log.stats().recordsDropped);
}

void test_long_outage_drops_and_counts() {
  FakeClock clock;
  hal::MockStorage sd(1 << 20);
  logger::SurveyLogger log(sd, clock);
  log.begin();
  sd.setPresent(false);
  for (uint32_t i = 0; i < 100; ++i) log.append(rec(i));  // buffer holds ~16
  TEST_ASSERT_FALSE(log.ok());
  TEST_ASSERT_TRUE(log.stats().recordsDropped > 50);
  TEST_ASSERT_EQUAL_UINT32(100, log.stats().recordsBuffered + log.stats().recordsDropped);
}

void test_card_full() {
  FakeClock clock;
  hal::MockStorage sd(3000);  // header + a few records
  logger::SurveyLogger log(sd, clock);
  log.begin();
  for (uint32_t i = 0; i < 30; ++i) log.append(rec(i));
  log.flushNow();
  TEST_ASSERT_FALSE(log.ok());
  TEST_ASSERT_TRUE(log.stats().writeErrors >= 1);
}

void test_no_card_at_boot() {
  FakeClock clock;
  hal::MockStorage sd;
  sd.setPresent(false);
  logger::SurveyLogger log(sd, clock);
  TEST_ASSERT_FALSE(log.begin());
  log.append(rec(0));
  sd.setPresent(true);
  clock.us = 5000000;
  log.tick();
  TEST_ASSERT_TRUE(log.ok());
  TEST_ASSERT_EQUAL_UINT32(1, log.stats().recordsWritten);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_file_name);
  RUN_TEST(test_new_file_with_header_and_no_overwrite);
  RUN_TEST(test_buffers_until_time_flush);
  RUN_TEST(test_full_buffer_writes_block);
  RUN_TEST(test_rotation_keeps_every_record);
  RUN_TEST(test_card_removed_and_reinserted);
  RUN_TEST(test_long_outage_drops_and_counts);
  RUN_TEST(test_card_full);
  RUN_TEST(test_no_card_at_boot);
  return UNITY_END();
}
