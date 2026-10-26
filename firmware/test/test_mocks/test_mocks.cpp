#include <unity.h>

#include <cmath>
#include <cstring>

#include "mock_gnss.h"
#include "mock_route.h"
#include "mock_sensors.h"
#include "mock_storage.h"
#include "nmea_parser.h"

class FakeClock : public hal::Clock {
 public:
  uint64_t us = 0;
  uint64_t nowUs() override { return us; }
};

void setUp() {}
void tearDown() {}

static double metres(double lat1, double lon1, double lat2, double lon2) {
  double dn = (lat2 - lat1) * 111320.0;
  double de = (lon2 - lon1) * 111320.0 * std::cos(lat1 * 3.14159265358979 / 180.0);
  return std::hypot(de, dn);
}

// Runs the mock GNSS for `seconds`, feeding its bytes through the real parser.
struct GnssRun {
  int epochs = 0, fixEpochs = 0, checksumErrors = 0;
  double maxErrM = 0, sumSqErr = 0;
  int firstFixS = -1;
  bool lostFixAfterFirst = false;
};

static GnssRun runGnss(const hal::MockGnssConfig& cfg, uint32_t seconds) {
  FakeClock clock;
  hal::MockRoute route;
  hal::MockGnss gnss(clock, route, cfg);
  nmea::Parser parser;
  GnssRun r;
  gnss.begin(9600);
  for (uint32_t s = 0; s < seconds; ++s) {
    clock.us = s * 1000000ULL + 500000ULL;  // mid-second, like a 1 Hz poll
    char buf[128];
    size_t n;
    while ((n = gnss.read(buf, sizeof(buf))) > 0)
      for (size_t i = 0; i < n; ++i)
        if (parser.feed(buf[i]) == nmea::Sentence::GGA) {
          ++r.epochs;
          const nmea::GnssData& d = parser.data();
          if (d.posValid) {
            ++r.fixEpochs;
            if (r.firstFixS < 0) r.firstFixS = static_cast<int>(s);
            hal::RoutePose t = gnss.truthAt(s);
            double err = metres(t.lat, t.lon, d.lat, d.lon);
            r.sumSqErr += err * err;
            if (err > r.maxErrM) r.maxErrM = err;
          } else if (r.firstFixS >= 0) {
            r.lostFixAfterFirst = true;
          }
        }
  }
  r.checksumErrors = static_cast<int>(parser.stats().checksumErrors);
  return r;
}

void test_route_geometry() {
  hal::MockRoute route;
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 200 + 120 + 126.5 + 89.4 + 120, route.lengthM());
  hal::RoutePose p0 = route.poseAt(0), pLoop = route.poseAt(route.lengthM());
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, p0.lat, pLoop.lat);
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 90.0, route.poseAt(10).headingDeg);  // first leg goes east
  hal::RoutePose p100 = route.poseAt(100);
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 100.0, metres(p0.lat, p0.lon, p100.lat, p100.lon));
}

void test_gnss_cold_start_noise_and_outage() {
  hal::MockGnssConfig cfg;  // defaults: fix after 10 s, outage 12 s every 180 s
  GnssRun r = runGnss(cfg, 400);
  TEST_ASSERT_EQUAL_INT(0, r.checksumErrors);
  TEST_ASSERT_EQUAL_INT(400, r.epochs);
  TEST_ASSERT_EQUAL_INT(10, r.firstFixS);
  TEST_ASSERT_TRUE(r.lostFixAfterFirst);
  TEST_ASSERT_EQUAL_INT(400 - 10 - 2 * 12, r.fixEpochs);  // outages at 90..101 and 270..281
  double rmse = std::sqrt(r.sumSqErr / r.fixEpochs);
  TEST_ASSERT_TRUE(rmse > 1.0 && rmse < 4.5);  // sqrt(2)*1.5 m noise + bias
  TEST_ASSERT_TRUE(r.maxErrM < 12.0);
}

void test_gnss_is_deterministic_per_seed() {
  hal::MockGnssConfig cfg;
  GnssRun a = runGnss(cfg, 60), b = runGnss(cfg, 60);
  TEST_ASSERT_EQUAL_DOUBLE(a.sumSqErr, b.sumSqErr);
  cfg.seed = 2;
  GnssRun c = runGnss(cfg, 60);
  TEST_ASSERT_TRUE(a.sumSqErr != c.sumSqErr);
}

void test_gnss_silent_module() {
  hal::MockGnssConfig cfg;
  cfg.silent = true;
  TEST_ASSERT_EQUAL_INT(0, runGnss(cfg, 20).epochs);
}

void test_gnss_does_not_flood_after_pause() {
  FakeClock clock;
  hal::MockRoute route;
  hal::MockGnss gnss(clock, route);
  gnss.begin(9600);
  clock.us = 100ULL * 1000000ULL;  // reader stalled for 100 s
  nmea::Parser parser;
  char buf[1024];
  size_t n = gnss.read(buf, sizeof(buf));
  for (size_t i = 0; i < n; ++i) parser.feed(buf[i]);
  TEST_ASSERT_TRUE(parser.stats().gga <= 3);
  TEST_ASSERT_EQUAL_UINT32(0, parser.stats().checksumErrors);
}

void test_imu_stationary_statistics() {
  FakeClock clock;
  hal::MockImu imu(clock, 7, hal::MockFaults(), false);
  TEST_ASSERT_EQUAL(hal::Status::Ok, imu.begin());
  double sum = 0, sumSq = 0;
  const int n = 500;
  for (int i = 0; i < n; ++i) {
    clock.us += 10000;
    hal::ImuSample s;
    TEST_ASSERT_EQUAL(hal::Status::Ok, imu.read(s));
    sum += s.az;
    sumSq += s.az * s.az;
  }
  double mean = sum / n, sd = std::sqrt(sumSq / n - mean * mean);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 9.81, mean);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.03, sd);
}

void test_fault_injection() {
  FakeClock clock;
  hal::MockFaults notFound;
  notFound.notFound = true;
  hal::MockEnv missing(clock, 1, notFound);
  hal::EnvSample e;
  TEST_ASSERT_EQUAL(hal::Status::NotFound, missing.begin());
  TEST_ASSERT_EQUAL(hal::Status::NotFound, missing.read(e));

  hal::MockFaults loose;
  loose.failAfterS = 5;
  hal::MockEnv env(clock, 1, loose);
  TEST_ASSERT_EQUAL(hal::Status::Ok, env.begin());
  clock.us = 4000000;
  TEST_ASSERT_EQUAL(hal::Status::Ok, env.read(e));
  TEST_ASSERT_FLOAT_WITHIN(3.0f, 18.0f, e.tempC);
  clock.us = 6000000;
  TEST_ASSERT_EQUAL(hal::Status::ReadError, env.read(e));

  hal::MockFaults flaky;
  flaky.readErrorRate = 0.2f;
  hal::MockImu imu(clock, 3, flaky);
  imu.begin();
  int errors = 0;
  hal::ImuSample s;
  for (int i = 0; i < 1000; ++i) errors += imu.read(s) == hal::Status::ReadError;
  TEST_ASSERT_INT_WITHIN(50, 200, errors);
}

void test_range_is_non_blocking() {
  FakeClock clock;
  hal::MockRange tof(clock, 5, hal::MockFaults(), 100, 0.0f);
  tof.begin();
  hal::RangeSample s;
  TEST_ASSERT_EQUAL(hal::Status::NotReady, tof.poll(s));
  clock.us = 100000;
  TEST_ASSERT_EQUAL(hal::Status::Ok, tof.poll(s));
  TEST_ASSERT_TRUE(s.valid);
  TEST_ASSERT_UINT16_WITHIN(100, 1500, s.mm);
  TEST_ASSERT_EQUAL(hal::Status::NotReady, tof.poll(s));
}

void test_storage_full_and_removed() {
  hal::MockStorage sd(10);
  TEST_ASSERT_TRUE(sd.begin());
  TEST_ASSERT_TRUE(sd.open("/a.csv"));
  TEST_ASSERT_EQUAL_UINT(6, sd.write("abcdef", 6));
  TEST_ASSERT_EQUAL_UINT(4, sd.write("ghijkl", 6));  // short write: card full
  TEST_ASSERT_EQUAL_STRING("abcdefghij", sd.contents("/a.csv").c_str());
  sd.setPresent(false);
  TEST_ASSERT_EQUAL_UINT(0, sd.write("x", 1));
  TEST_ASSERT_FALSE(sd.open("/b.csv"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_route_geometry);
  RUN_TEST(test_gnss_cold_start_noise_and_outage);
  RUN_TEST(test_gnss_is_deterministic_per_seed);
  RUN_TEST(test_gnss_silent_module);
  RUN_TEST(test_gnss_does_not_flood_after_pause);
  RUN_TEST(test_imu_stationary_statistics);
  RUN_TEST(test_fault_injection);
  RUN_TEST(test_range_is_non_blocking);
  RUN_TEST(test_storage_full_and_removed);
  return UNITY_END();
}
