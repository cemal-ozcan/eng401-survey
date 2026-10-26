// Consumer and housekeeping tasks. logTask and netTask are placeholders here:
// step 5 adds buffered SD logging, step 6 Wi-Fi telemetry.

#include <Arduino.h>

#include "config.h"
#include "pins.h"
#include "debouncer.h"
#include "shared_state.h"
#include "tasks.h"

namespace app {

void logTask(void*) {
  record::Record r;
  char line[record::kMaxCsvLen];
  serialPrintf("%s", record::csvHeader());
  for (;;) {
    // Blocking on a queue is not busy-waiting: the task sleeps until data arrives.
    if (xQueueReceive(logQueue, &r, portMAX_DELAY) != pdTRUE) continue;
    if (record::toCsv(r, line, sizeof(line)) > 0) serialPrintf("%s", line);
    lastLogUs = hal::devices().clock.nowUs();
    ++recordsLogged;
  }
}

void netTask(void*) {
  record::Record r;
  for (;;) xQueueReceive(netQueue, &r, portMAX_DELAY);  // discarded until step 6
}

void uiTask(void*) {
  hal::Devices& d = hal::devices();
  pinMode(pins::kLedFix, OUTPUT);
  pinMode(pins::kLedLog, OUTPUT);
  pinMode(pins::kLedError, OUTPUT);
  pinMode(pins::kButton, INPUT_PULLUP);

  rt::Debouncer button;
  uint16_t markCounter = 0;
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    const uint64_t now = d.clock.nowUs();
    if (button.update(digitalRead(pins::kButton) == LOW, now)) {
      pendingMark = ++markCounter;
      serialPrintf("# MARK %u", markCounter);
    }

    // Green: solid = GNSS fix, slow blink = time known but no fix, off = nothing.
    nmea::GnssData g;
    uint64_t gUs;
    shared.getGnss(g, gUs);
    bool blink = (now / 500000) % 2;
    digitalWrite(pins::kLedFix, g.posValid ? HIGH : (g.timeValid && blink ? HIGH : LOW));
    // Blue: short flash for every record written.
    digitalWrite(pins::kLedLog, now - lastLogUs.load() < 80000 ? HIGH : LOW);
    // Red: any sensor failed or storage problem.
    SensorHealth h = shared.health();
    bool error = !h.imu.ok() || !h.env.ok() || !h.range.ok() || !storageOk.load();
    digitalWrite(pins::kLedError, error ? HIGH : LOW);

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(config::kUiLoopMs));
  }
}

}  // namespace app
