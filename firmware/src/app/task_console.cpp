// Serial console: field configuration without recompiling (see lib/console).

#include <Arduino.h>

#include "console.h"
#include "settings_store.h"
#include "shared_state.h"
#include "tasks.h"

namespace app {

namespace {

void printHelp() {
  serialPrintf("# CMD commands: help | show | wifi \"<ssid>\" \"<password>\" | host <ip> | reset-settings | reboot");
}

void printSettings(const console::Settings& s, const char* title) {
  serialPrintf("# CMD %s: wifi ssid=\"%s\" password=%s host=%s", title, s.ssid,
               s.password[0] ? "(set)" : "(none)", s.host);
}

}  // namespace

void consoleTask(void*) {
  char line[160];
  size_t len = 0;
  console::Settings pending = settings;
  serialPrintf("# CMD console ready, type 'help' and press Enter");
  for (;;) {
    while (Serial.available() > 0) {
      char c = static_cast<char>(Serial.read());
      if (c != '\r' && c != '\n') {
        if (len < sizeof(line) - 1) line[len++] = c;
        continue;
      }
      if (len == 0) continue;
      line[len] = '\0';
      len = 0;
      console::Command cmd = console::execute(line, pending);
      switch (cmd.type) {
        case console::CommandType::Help: printHelp(); break;
        case console::CommandType::Show:
          printSettings(settings, "active");
          printSettings(pending, "saved");
          break;
        case console::CommandType::Wifi:
        case console::CommandType::Host:
        case console::CommandType::ResetSettings:
          if (saveSettings(pending)) {
            printSettings(pending, "saved");
            serialPrintf("# CMD type 'reboot' to apply");
          } else {
            serialPrintf("# CMD ERROR could not write settings to flash");
          }
          break;
        case console::CommandType::Reboot:
          serialPrintf("# CMD flushing SD card and rebooting...");
          flushRequested = true;
          vTaskDelay(pdMS_TO_TICKS(1500));  // logTask flushes within its 500 ms wake-up
          ESP.restart();
          break;
        case console::CommandType::Invalid: serialPrintf("# CMD ERROR %s", cmd.error); break;
        case console::CommandType::Empty: break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

}  // namespace app
