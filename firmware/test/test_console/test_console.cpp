#include <unity.h>

#include <cstring>

#include "console.h"

void setUp() {}
void tearDown() {}

static console::Command run(const char* text, console::Settings& s) {
  static char buf[160];
  std::strncpy(buf, text, sizeof(buf) - 1);
  return console::execute(buf, s);
}

void test_tokenize_quotes_and_spaces() {
  char line[] = "  wifi   \"My Phone\"  secret123 ";
  char* t[4];
  TEST_ASSERT_EQUAL_INT(3, console::tokenize(line, t, 4));
  TEST_ASSERT_EQUAL_STRING("wifi", t[0]);
  TEST_ASSERT_EQUAL_STRING("My Phone", t[1]);
  TEST_ASSERT_EQUAL_STRING("secret123", t[2]);
  char bad[] = "wifi \"open";
  TEST_ASSERT_EQUAL_INT(-1, console::tokenize(bad, t, 4));
}

void test_wifi_command_sets_credentials() {
  console::Settings s;
  TEST_ASSERT_EQUAL(console::CommandType::Wifi, run("wifi \"Ali's iPhone\" \"pass word!\"", s).type);
  TEST_ASSERT_EQUAL_STRING("Ali's iPhone", s.ssid);
  TEST_ASSERT_EQUAL_STRING("pass word!", s.password);
  TEST_ASSERT_EQUAL(console::CommandType::Wifi, run("wifi CampusGuest", s).type);
  TEST_ASSERT_EQUAL_STRING("", s.password);
}

void test_wifi_validation_keeps_old_values() {
  console::Settings s;
  run("wifi Good goodpass1", s);
  console::Command c = run("wifi Good short", s);
  TEST_ASSERT_EQUAL(console::CommandType::Invalid, c.type);
  TEST_ASSERT_NOT_NULL(c.error);
  TEST_ASSERT_EQUAL_STRING("goodpass1", s.password);
  TEST_ASSERT_EQUAL(console::CommandType::Invalid, run("wifi", s).type);
  TEST_ASSERT_EQUAL(console::CommandType::Invalid,
                    run("wifi 123456789012345678901234567890123 password1", s).type);  // 33 chars
}

void test_host_and_ipv4() {
  console::Settings s;
  TEST_ASSERT_EQUAL(console::CommandType::Host, run("host 192.168.43.20", s).type);
  TEST_ASSERT_EQUAL_STRING("192.168.43.20", s.host);
  TEST_ASSERT_EQUAL(console::CommandType::Invalid, run("host 192.168.43", s).type);
  TEST_ASSERT_FALSE(console::isValidIpv4("256.1.1.1"));
  TEST_ASSERT_FALSE(console::isValidIpv4("1.2.3.4.5"));
  TEST_ASSERT_FALSE(console::isValidIpv4("a.b.c.d"));
  TEST_ASSERT_TRUE(console::isValidIpv4("255.255.255.255"));
}

void test_other_commands() {
  console::Settings s;
  run("wifi X password1", s);
  TEST_ASSERT_EQUAL(console::CommandType::ResetSettings, run("reset-settings", s).type);
  TEST_ASSERT_EQUAL_STRING("", s.ssid);
  TEST_ASSERT_EQUAL(console::CommandType::Empty, run("   ", s).type);
  TEST_ASSERT_EQUAL(console::CommandType::Help, run("help", s).type);
  TEST_ASSERT_EQUAL(console::CommandType::Invalid, run("format-disk", s).type);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tokenize_quotes_and_spaces);
  RUN_TEST(test_wifi_command_sets_credentials);
  RUN_TEST(test_wifi_validation_keeps_old_values);
  RUN_TEST(test_host_and_ipv4);
  RUN_TEST(test_other_commands);
  return UNITY_END();
}
