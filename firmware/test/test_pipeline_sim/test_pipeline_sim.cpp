// End-to-end host simulation of the acquisition chain with mock devices:
//   MockGnss -> NMEA parser -> TimeBase ; mock sensors -> record_builder ->
//   SurveyLogger -> MockStorage
// Checks data completeness of a 10-minute survey. With SIM_OUT=<dir> set, it
// also writes the survey CSV and the ground-truth trajectory (for PC tests):
//   SIM_OUT=../data/sample ../.venv/bin/pio test -e native -f test_pipeline_sim

#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "mock_gnss.h"
#include "mock_route.h"
#include "mock_sensors.h"
#include "mock_storage.h"
#include "nmea_parser.h"
#include "record_builder.h"
#include "survey_logger.h"
#include "timebase.h"

class FakeClock : public hal::Clock {
 public:
  uint64_t us = 0;
  uint64_t nowUs() override { return us; }
};

void setUp() {}
void tearDown() {}

static constexpr uint32_t kSeconds = 600;

void test_ten_minute_survey() {
  FakeClock clock;
  hal::MockRoute route;
  hal::MockGnssConfig gcfg;  // cold start 10 s, outage 12 s every 180 s
  hal::MockGnss gnss(clock, route, gcfg);
  hal::MockImu imu(clock, 11);
  hal::MockFaults envFaults;
  envFaults.readErrorRate = 0.02f;  // occasional BME280 glitch
  hal::MockEnv env(clock, 12, envFaults);
  hal::MockRange range(clock, 13);
  hal::MockStorage sd(4u << 20);
  logger::SurveyLogger log(sd, clock);

  nmea::Parser parser;
  rt::TimeBase timeBase;
  uint64_t gnssUpdateUs = 0;

  gnss.begin(9600);
  imu.begin();
  env.begin();
  range.begin();
  TEST_ASSERT_TRUE(log.begin());

  int fixRows = 0, utcRows = 0;
  int64_t maxUtcErrMs = 0;
  hal::RangeSample lastRange{0, false};
  bool rangeFresh = false;

  for (uint32_t s = 0; s < kSeconds; ++s) {
    // 20 ms poll loop; GNSS output appears outputLagMs after each whole second.
    for (uint32_t ms = 0; ms < 1000; ms += 20) {
      clock.us = s * 1000000ULL + ms * 1000ULL;
      char buf[128];
      size_t n;
      while ((n = gnss.read(buf, sizeof(buf))) > 0)
        for (size_t i = 0; i < n; ++i) {
          nmea::Sentence st = parser.feed(buf[i]);
          if (st == nmea::Sentence::GGA || st == nmea::Sentence::RMC) {
            gnssUpdateUs = clock.us;
            timeBase.addSample(parser.data().utcMs(), clock.us);
          }
        }
      hal::RangeSample rs;
      if (range.poll(rs) == hal::Status::Ok) {
        lastRange = rs;
        rangeFresh = true;
      }
      if (ms == 500) {  // sampling tick in the middle of the second
        record::Record r;
        r.seq = s;
        r.tUs = clock.us;
        r.utcMs = timeBase.toUtcMs(clock.us);
        if (r.utcMs >= 0) {
          r.flags |= record::kFlagUtcSynced;
          ++utcRows;
          // Mock receiver time is exact: utc = start + clock (epoch on whole seconds).
          int64_t truth = gcfg.startUtcMs + static_cast<int64_t>(clock.us / 1000);
          int64_t err = std::llabs(r.utcMs - truth);
          if (err > maxUtcErrMs) maxUtcErrMs = err;
        }
        r.flags |= record::kFlagMock | record::kFlagSdOk;
        record::fillGnss(r, parser.data(), gnssUpdateUs, clock.us);
        hal::ImuSample is;
        record::fillImu(r, imu.read(is) == hal::Status::Ok ? &is : nullptr);
        hal::EnvSample es;
        record::fillEnv(r, env.read(es) == hal::Status::Ok ? &es : nullptr);
        record::fillRange(r, rangeFresh ? &lastRange : nullptr);
        rangeFresh = false;
        fixRows += (r.flags & record::kFlagGnssFix) != 0;
        log.append(r);
      }
      log.tick();
    }
  }
  log.flushNow();

  const std::string& csv = sd.contents(log.fileName());
  int lines = 0;
  for (char c : csv) lines += c == '\n';
  TEST_ASSERT_EQUAL_INT(kSeconds + 1, lines);  // header + one row per second
  TEST_ASSERT_EQUAL_UINT32(kSeconds, log.stats().recordsWritten);
  TEST_ASSERT_EQUAL_UINT32(0, log.stats().recordsDropped);
  // Fix: lost during the first 10 s and in 3 outages of 12 s (at 90, 270, 450 s);
  // the GNSS solution sampled at x.5 s belongs to second x.
  TEST_ASSERT_EQUAL_INT(kSeconds - 10 - 3 * 12, fixRows);
  TEST_ASSERT_TRUE(utcRows >= static_cast<int>(kSeconds) - 3);
  // Without PPS the time base cannot see the receiver's output latency: the
  // converted UTC is late-biased by exactly that lag (documented limitation).
  TEST_ASSERT_TRUE(maxUtcErrMs <= static_cast<int64_t>(gcfg.outputLagMs));
  std::printf("sim: %d rows, %d with fix, %d with UTC, max UTC error %lld ms, file %s\n", lines - 1, fixRows,
              utcRows, static_cast<long long>(maxUtcErrMs), log.fileName());

  const char* outDir = std::getenv("SIM_OUT");
  if (outDir != nullptr) {
    std::string path = std::string(outDir) + "/mock_survey.csv";
    FILE* f = std::fopen(path.c_str(), "w");
    TEST_ASSERT_NOT_NULL(f);
    std::fwrite(csv.data(), 1, csv.size(), f);
    std::fclose(f);

    path = std::string(outDir) + "/mock_truth.csv";
    f = std::fopen(path.c_str(), "w");
    TEST_ASSERT_NOT_NULL(f);
    std::fprintf(f, "seq,lat,lon\n");
    for (uint32_t s = 0; s < kSeconds; ++s) {
      hal::RoutePose p = gnss.truthAt(s);
      std::fprintf(f, "%u,%.8f,%.8f\n", s, p.lat, p.lon);
    }
    std::fclose(f);
    std::printf("sim: wrote %s/mock_survey.csv and mock_truth.csv\n", outDir);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ten_minute_survey);
  return UNITY_END();
}
