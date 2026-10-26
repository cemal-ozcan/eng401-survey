#include "console.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace console {

int tokenize(char* line, char** tokens, int maxTokens) {
  int n = 0;
  char* p = line;
  while (*p) {
    while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    if (!*p) break;
    if (n >= maxTokens) return n;
    if (*p == '"') {
      char* start = ++p;
      while (*p && *p != '"') ++p;
      if (*p != '"') return -1;
      *p++ = '\0';
      tokens[n++] = start;
    } else {
      tokens[n++] = p;
      while (*p && !std::isspace(static_cast<unsigned char>(*p))) ++p;
      if (*p) *p++ = '\0';
    }
  }
  return n;
}

bool isValidIpv4(const char* s) {
  int parts = 0;
  const char* p = s;
  while (parts < 4) {
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    char* end = nullptr;
    long v = std::strtol(p, &end, 10);
    if (v < 0 || v > 255 || end - p > 3) return false;
    ++parts;
    p = end;
    if (parts < 4) {
      if (*p != '.') return false;
      ++p;
    }
  }
  return *p == '\0';
}

// (C++11 on the ESP32 toolchain: no brace-init of structs with member defaults)
static Command make(CommandType type, const char* error = nullptr) {
  Command c;
  c.type = type;
  c.error = error;
  return c;
}

static Command invalid(const char* why) { return make(CommandType::Invalid, why); }

Command execute(char* line, Settings& settings) {
  char* t[4];
  int n = tokenize(line, t, 4);
  if (n < 0) return invalid("missing closing quote");
  if (n == 0) return make(CommandType::Empty);

  if (std::strcmp(t[0], "help") == 0) return make(CommandType::Help);
  if (std::strcmp(t[0], "show") == 0) return make(CommandType::Show);
  if (std::strcmp(t[0], "reboot") == 0) return make(CommandType::Reboot);
  if (std::strcmp(t[0], "reset-settings") == 0) {
    settings = Settings();
    return make(CommandType::ResetSettings);
  }
  if (std::strcmp(t[0], "wifi") == 0) {
    if (n < 2 || n > 3) return invalid("usage: wifi \"<ssid>\" \"<password>\"");
    size_t ssidLen = std::strlen(t[1]);
    const char* pass = n == 3 ? t[2] : "";
    size_t passLen = std::strlen(pass);
    if (ssidLen == 0 || ssidLen > kMaxSsid) return invalid("SSID must be 1-32 characters");
    if (passLen != 0 && (passLen < kMinPassword || passLen > kMaxPassword))
      return invalid("password must be 8-63 characters (or omitted for an open network)");
    std::snprintf(settings.ssid, sizeof(settings.ssid), "%s", t[1]);
    std::snprintf(settings.password, sizeof(settings.password), "%s", pass);
    return make(CommandType::Wifi);
  }
  if (std::strcmp(t[0], "host") == 0) {
    if (n != 2 || !isValidIpv4(t[1])) return invalid("usage: host <a.b.c.d>");
    std::snprintf(settings.host, sizeof(settings.host), "%s", t[1]);
    return make(CommandType::Host);
  }
  return invalid("unknown command, type: help");
}

}  // namespace console
