#include "settings_store.h"

#include <Preferences.h>

#include <cstdio>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace app {

namespace {
constexpr const char* kNamespace = "eng401";

void defaults(console::Settings& s) {
  s = console::Settings();
#ifdef WIFI_SSID
  std::snprintf(s.ssid, sizeof(s.ssid), "%s", WIFI_SSID);
  std::snprintf(s.password, sizeof(s.password), "%s", WIFI_PASSWORD);
#endif
#ifdef TELEMETRY_DEFAULT_HOST
  std::snprintf(s.host, sizeof(s.host), "%s", TELEMETRY_DEFAULT_HOST);
#endif
}
}  // namespace

console::Settings settings;

void loadSettings(console::Settings& out) {
  defaults(out);
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return;  // read-only; fails if never written
  if (prefs.isKey("ssid")) {
    prefs.getString("ssid", out.ssid, sizeof(out.ssid));
    prefs.getString("pass", out.password, sizeof(out.password));
  }
  if (prefs.isKey("host")) prefs.getString("host", out.host, sizeof(out.host));
  prefs.end();
}

bool saveSettings(const console::Settings& s) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  bool ok = prefs.putString("ssid", s.ssid) > 0 || s.ssid[0] == '\0';
  prefs.putString("pass", s.password);
  ok = ok && prefs.putString("host", s.host) > 0;
  prefs.end();
  return ok;
}

}  // namespace app
