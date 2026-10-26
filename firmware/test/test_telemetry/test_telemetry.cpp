#include <unity.h>

#include "telemetry.h"

void setUp() {}
void tearDown() {}

void test_heartbeat_format() {
  telemetry::Heartbeat hb{43100000, 43, 182344, -61, 7420, 0, 1, 2, "0.2.0"};
  char buf[256];
  TEST_ASSERT_TRUE(telemetry::encodeHeartbeat(hb, buf, sizeof(buf)) > 0);
  TEST_ASSERT_EQUAL_STRING(
      "{\"v\":1,\"type\":\"hb\",\"t_us\":43100000,\"uptime_s\":43,\"heap\":182344,\"rssi\":-61,"
      "\"sd_free_mb\":7420,\"q_drop\":0,\"sd_err\":1,\"net_drop\":2,\"fw\":\"0.2.0\"}",
      buf);
  char small[20];
  TEST_ASSERT_EQUAL_UINT(0, telemetry::encodeHeartbeat(hb, small, sizeof(small)));
}

void test_ping_parse_and_pong() {
  telemetry::Ping p;
  TEST_ASSERT_TRUE(telemetry::parsePing("{\"v\":1,\"type\":\"ping\",\"id\":7,\"t1\":1700000000123456}", p));
  TEST_ASSERT_EQUAL_UINT32(7, p.id);
  TEST_ASSERT_EQUAL_INT64(1700000000123456LL, p.t1);
  char buf[160];
  TEST_ASSERT_TRUE(telemetry::encodePong(p, 5000000, 5000150, buf, sizeof(buf)) > 0);
  TEST_ASSERT_EQUAL_STRING(
      "{\"v\":1,\"type\":\"pong\",\"id\":7,\"t1\":1700000000123456,\"t2\":5000000,\"t3\":5000150}", buf);
}

void test_ping_rejects_other_packets() {
  telemetry::Ping p;
  TEST_ASSERT_FALSE(telemetry::parsePing("{\"v\":1,\"type\":\"hb\",\"id\":7,\"t1\":1}", p));
  TEST_ASSERT_FALSE(telemetry::parsePing("{\"v\":2,\"type\":\"ping\",\"id\":7,\"t1\":1}", p));
  TEST_ASSERT_FALSE(telemetry::parsePing("{\"v\":1,\"type\":\"ping\",\"id\":7}", p));
  TEST_ASSERT_FALSE(telemetry::parsePing("garbage", p));
}

void test_json_get_int_does_not_confuse_keys() {
  int64_t v;
  // "t1" must not match inside "xt1"
  TEST_ASSERT_TRUE(telemetry::jsonGetInt("{\"xt1\":5,\"t1\":-9}", "t1", v));
  TEST_ASSERT_EQUAL_INT64(-9, v);
  TEST_ASSERT_FALSE(telemetry::jsonGetInt("{\"t1\":\"a\"}", "t1", v));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_heartbeat_format);
  RUN_TEST(test_ping_parse_and_pong);
  RUN_TEST(test_ping_rejects_other_packets);
  RUN_TEST(test_json_get_int_does_not_confuse_keys);
  return UNITY_END();
}
