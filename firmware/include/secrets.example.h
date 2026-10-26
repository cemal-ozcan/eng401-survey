#pragma once

// Copy this file to secrets.h (git-ignored) and fill in your hotspot details.
// Without secrets.h the firmware builds with these placeholders.

#define WIFI_SSID "ENG401-hotspot"
#define WIFI_PASSWORD "change-me"

// Where telemetry goes until the PC introduces itself with a ping.
// 255.255.255.255 = broadcast to the whole hotspot network, so the PC's
// IP address does not have to be known in advance.
#define TELEMETRY_DEFAULT_HOST "255.255.255.255"
