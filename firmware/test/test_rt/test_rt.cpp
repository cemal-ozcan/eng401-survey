#include <unity.h>

#include <cmath>

#include "debouncer.h"
#include "interval_stats.h"
#include "supervisor.h"
#include "timebase.h"

void setUp() {}
void tearDown() {}

static constexpr int64_t kUtc0Ms = 1793613600000LL;  // 2026-11-02T10:00:00Z

// ---------------------------------------------------------------- TimeBase

void test_timebase_unsynced() {
  rt::TimeBase tb;
  TEST_ASSERT_FALSE(tb.synced());
  TEST_ASSERT_EQUAL_INT64(-1, tb.toUtcMs(123));
}

void test_timebase_picks_least_delayed_sample() {
  // True relation: utc = kUtc0 + local. Each epoch arrives 50..250 ms late;
  // epoch 7 arrives only 20 ms late.
  rt::TimeBase tb;
  const int lagMs[] = {120, 250, 80, 200, 150, 90, 230, 20, 160, 110};
  for (int s = 0; s < 10; ++s) {
    uint64_t local = static_cast<uint64_t>(s) * 1000000ULL + lagMs[s] * 1000ULL;
    tb.addSample(kUtc0Ms + s * 1000, local);
  }
  // Estimated offset is 20 ms too small -> converted time 20 ms early.
  TEST_ASSERT_EQUAL_INT64(kUtc0Ms + 5000 - 20, tb.toUtcMs(5000000ULL));
  TEST_ASSERT_EQUAL_INT64(230000, tb.spreadUs());
}

void test_timebase_window_forgets_old_samples() {
  rt::TimeBase tb;
  tb.addSample(kUtc0Ms, 0);  // perfect sample, then only 100 ms-late ones
  for (int s = 1; s <= rt::TimeBase::kWindow; ++s)
    tb.addSample(kUtc0Ms + s * 1000, s * 1000000ULL + 100000ULL);
  TEST_ASSERT_EQUAL_INT64(kUtc0Ms * 1000 - 100000, tb.offsetUs());
}

void test_timebase_resets_on_jump() {
  rt::TimeBase tb;
  tb.addSample(kUtc0Ms - 86400000LL, 0);  // receiver started with a wrong date
  tb.addSample(kUtc0Ms + 1000, 1050000);  // corrected one second later
  TEST_ASSERT_EQUAL_UINT32(1, tb.resets());
  TEST_ASSERT_EQUAL_INT64(kUtc0Ms + 1950, tb.toUtcMs(2000000));
}

// ---------------------------------------------------------------- IntervalStats

void test_interval_stats_and_missed() {
  rt::IntervalStats st(100000);  // 100 ms
  uint64_t t = 0;
  st.tick(t);
  for (int i = 0; i < 9; ++i) st.tick(t += (i % 2 ? 99000 : 101000));
  st.tick(t += 300000);  // two ticks skipped
  TEST_ASSERT_EQUAL_UINT32(10, st.intervals());
  TEST_ASSERT_EQUAL_UINT32(2, st.missed());
  TEST_ASSERT_EQUAL_UINT64(99000, st.minUs());
  TEST_ASSERT_EQUAL_UINT64(300000, st.maxUs());
  TEST_ASSERT_DOUBLE_WITHIN(1.0, (5 * 101000.0 + 4 * 99000.0 + 300000.0) / 10.0, st.meanUs());
  TEST_ASSERT_TRUE(st.stdUs() > 60000.0);
}

void test_interval_stats_jitter_is_not_missed() {
  rt::IntervalStats st(1000000);
  st.tick(0);
  st.tick(1400000);  // late but < 1.5 periods
  st.tick(2000000);
  TEST_ASSERT_EQUAL_UINT32(0, st.missed());
}

// ---------------------------------------------------------------- Supervisor

void test_supervisor_fails_after_threshold_and_backs_off() {
  rt::Supervisor sv;
  sv.onInit(hal::Status::Ok, 0);
  TEST_ASSERT_TRUE(sv.ok());
  sv.onRead(hal::Status::ReadError, 1);
  sv.onRead(hal::Status::ReadError, 2);
  TEST_ASSERT_TRUE(sv.ok());  // 2 errors: still OK
  sv.onRead(hal::Status::NotReady, 3);
  sv.onRead(hal::Status::ReadError, 4);
  TEST_ASSERT_FALSE(sv.ok());
  TEST_ASSERT_FALSE(sv.retryDue(4 + 999999));
  TEST_ASSERT_TRUE(sv.retryDue(4 + 1000000));

  sv.onInit(hal::Status::NotFound, 1000004);  // retry fails -> next wait 2 s
  TEST_ASSERT_FALSE(sv.retryDue(1000004 + 1999999));
  TEST_ASSERT_TRUE(sv.retryDue(1000004 + 2000000));
  sv.onInit(hal::Status::Ok, 3000004);
  TEST_ASSERT_TRUE(sv.ok());
  TEST_ASSERT_EQUAL_UINT32(2, sv.reinits());
  TEST_ASSERT_EQUAL_UINT32(3, sv.errors());
}

void test_supervisor_backoff_caps() {
  rt::Supervisor sv;
  uint64_t t = 0;
  for (int i = 0; i < 10; ++i) sv.onInit(hal::Status::NotFound, t);
  TEST_ASSERT_TRUE(sv.retryDue(t + rt::Supervisor::kMaxBackoffUs));
}

void test_supervisor_ok_read_resets_streak() {
  rt::Supervisor sv;
  sv.onInit(hal::Status::Ok, 0);
  for (int i = 0; i < 10; ++i) {
    sv.onRead(hal::Status::ReadError, i);
    sv.onRead(hal::Status::ReadError, i);
    sv.onRead(hal::Status::Ok, i);
  }
  TEST_ASSERT_TRUE(sv.ok());
}

// ---------------------------------------------------------------- Debouncer

void test_debouncer_ignores_bounce() {
  rt::Debouncer db(30000);
  int presses = 0;
  uint64_t t = 0;
  // bouncing contact for 10 ms, then held for 100 ms, then released with bounce
  const bool bounce[] = {true, false, true, false, true};
  for (bool b : bounce) presses += db.update(b, t += 2000);
  for (int i = 0; i < 50; ++i) presses += db.update(true, t += 2000);
  for (bool b : bounce) presses += db.update(!b, t += 2000);
  for (int i = 0; i < 50; ++i) presses += db.update(false, t += 2000);
  TEST_ASSERT_EQUAL_INT(1, presses);
  TEST_ASSERT_FALSE(db.isPressed());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_timebase_unsynced);
  RUN_TEST(test_timebase_picks_least_delayed_sample);
  RUN_TEST(test_timebase_window_forgets_old_samples);
  RUN_TEST(test_timebase_resets_on_jump);
  RUN_TEST(test_interval_stats_and_missed);
  RUN_TEST(test_interval_stats_jitter_is_not_missed);
  RUN_TEST(test_supervisor_fails_after_threshold_and_backs_off);
  RUN_TEST(test_supervisor_backoff_caps);
  RUN_TEST(test_supervisor_ok_read_resets_streak);
  RUN_TEST(test_debouncer_ignores_bounce);
  return UNITY_END();
}
