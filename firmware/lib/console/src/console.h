#pragma once

// Serial console for field configuration without recompiling (the hardware
// team cannot build firmware). Commands, typed in any serial monitor:
//
//   help                      list commands
//   show                      print current settings
//   wifi "<ssid>" "<pass>"    set hotspot (quotes needed only with spaces)
//   wifi "<ssid>"             open network (no password)
//   host <ipv4>               telemetry target before discovery (default broadcast)
//   reset-settings            back to the compiled defaults
//   reboot                    restart (new Wi-Fi settings apply after reboot)

#include <cstddef>
#include <cstdint>

namespace console {

constexpr size_t kMaxSsid = 32;      // IEEE 802.11 limit
constexpr size_t kMaxPassword = 63;  // WPA2 passphrase limit
constexpr size_t kMinPassword = 8;
constexpr size_t kMaxHost = 15;      // dotted IPv4

struct Settings {
  char ssid[kMaxSsid + 1] = "";
  char password[kMaxPassword + 1] = "";
  char host[kMaxHost + 1] = "255.255.255.255";
};

enum class CommandType : uint8_t { Empty, Help, Show, Wifi, Host, ResetSettings, Reboot, Invalid };

struct Command {
  CommandType type = CommandType::Empty;
  const char* error = nullptr;  // set for Invalid
};

// Splits a line into at most maxTokens tokens in place. Double quotes group
// words ("My Phone") and are removed. Returns the token count, or -1 for an
// unterminated quote.
int tokenize(char* line, char** tokens, int maxTokens);

// Parses a line and applies wifi/host changes to `settings` (only if valid).
Command execute(char* line, Settings& settings);

bool isValidIpv4(const char* s);

}  // namespace console
