#pragma once

// OPTIONAL. Copy to secrets.h (git-ignored) to compile default Wi-Fi settings
// into the firmware. Normally not needed: set Wi-Fi at run time from the
// serial console with   wifi "<ssid>" "<password>"   and   reboot.

#define WIFI_SSID "ENG401-hotspot"
#define WIFI_PASSWORD "change-me"
#define TELEMETRY_DEFAULT_HOST "255.255.255.255"
