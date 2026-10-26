// Consumer and housekeeping tasks: SD logging and the user interface.
// Telemetry lives in task_net.cpp.

#include <Arduino.h>

#include "config.h"
#include "pins.h"
#include "debouncer.h"
#include "shared_state.h"
#include "survey_logger.h"
#include "tasks.h"

namespace app {

void logTask(void*) {
  hal::Devices& d = hal::devices();
  logger::SurveyLogger log(d.storage, d.clock);
  log.begin();
  storageOk = log.ok();
  serialPrintf("# LOG file %s %s", log.fileName(), log.ok() ? "OK" : "FAIL (will retry)");
  if (config::kEchoCsvToSerial) serialPrintf("%s", record::csvHeader());

  record::Record r;
  char line[record::kMaxCsvLen];
  for (;;) {
    // Wait for a record, but wake up at least every 500 ms so time-based
    // flushes and SD retries still happen when no data arrives.
    if (xQueueReceive(logQueue, &r, pdMS_TO_TICKS(500)) == pdTRUE) {
      log.append(r);
      lastLogUs = d.clock.nowUs();
      ++recordsLogged;
      if (config::kEchoCsvToSerial && record::toCsv(r, line, sizeof(line)) > 0) serialPrintf("%s", line);
    }
    log.tick();
    if (storageOk.load() != log.ok()) {
      storageOk = log.ok();
      serialPrintf("# LOG %s file %s", log.ok() ? "recovered," : "ERROR, retrying;", log.fileName());
    }
    shared.lock();  // struct copy shared with monitorTask
    loggerStats = log.stats();
    shared.unlock();
  }
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
