#pragma once

// Persistent device settings in NVS flash (survive reboots and re-flashing of
// the same partition layout). Defaults come from secrets.h if present.

#include "console.h"

namespace app {

void loadSettings(console::Settings& out);
bool saveSettings(const console::Settings& s);

// Settings snapshot taken at boot (Wi-Fi changes apply after a reboot).
extern console::Settings settings;

}  // namespace app
