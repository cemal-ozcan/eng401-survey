// Periodic health report on the serial port (lines start with '#', so CSV
// readers can skip them). Shows the timing statistics required by the handout
// (requested vs actual interval) and stack usage of every task.

#include <Arduino.h>

#include "config.h"
#include "shared_state.h"
#include "tasks.h"

namespace app {

namespace {
const char* sensorState(const rt::Supervisor& s) { return s.ok() ? "OK" : "FAIL"; }
}  // namespace

void monitorTask(void* taskHandles) {
  TaskHandle_t* handles = static_cast<TaskHandle_t*>(taskHandles);
  static const char* const kNames[] = {"gnss", "sensor", "sample", "log", "net", "ui"};
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(config::kMonitorPeriodMs));
    rt::IntervalStats st = shared.stats();
    rt::TimeBase tb = shared.timeBase();
    SensorHealth h = shared.health();
    serialPrintf("# STAT up=%lus rec=%lu logged=%lu qdrop=%lu interval(ms) req=%lu mean=%.2f min=%.2f "
                 "max=%.2f std=%.3f missed=%lu",
                 static_cast<unsigned long>(millis() / 1000), static_cast<unsigned long>(recordsProduced.load()),
                 static_cast<unsigned long>(recordsLogged.load()), static_cast<unsigned long>(queueDrops.load()),
                 static_cast<unsigned long>(config::kSamplePeriodMs), st.meanUs() / 1000.0, st.minUs() / 1000.0,
                 st.maxUs() / 1000.0, st.stdUs() / 1000.0, static_cast<unsigned long>(st.missed()));
    serialPrintf("# STAT utc_synced=%d tb_samples=%lu tb_spread_ms=%.1f imu=%s(err %lu) env=%s(err %lu) "
                 "range=%s(err %lu) heap=%lu",
                 tb.synced(), static_cast<unsigned long>(tb.samples()), tb.spreadUs() / 1000.0,
                 sensorState(h.imu), static_cast<unsigned long>(h.imu.errors()), sensorState(h.env),
                 static_cast<unsigned long>(h.env.errors()), sensorState(h.range),
                 static_cast<unsigned long>(h.range.errors()), static_cast<unsigned long>(ESP.getFreeHeap()));
    shared.lock();
    logger::Stats ls = loggerStats;
    shared.unlock();
    serialPrintf("# STAT sd=%s written=%lu dropped=%lu sd_errors=%lu files=%lu kb=%lu max_write_ms=%.1f",
                 storageOk.load() ? "OK" : "FAIL", static_cast<unsigned long>(ls.recordsWritten),
                 static_cast<unsigned long>(ls.recordsDropped), static_cast<unsigned long>(ls.writeErrors),
                 static_cast<unsigned long>(ls.filesOpened), static_cast<unsigned long>(ls.bytesWritten / 1024),
                 ls.maxWriteUs / 1000.0);
    char stacks[160] = "# STACK free(bytes)";
    for (int i = 0; i < 6; ++i) {
      char item[24];
      snprintf(item, sizeof(item), " %s=%u", kNames[i],
               static_cast<unsigned>(uxTaskGetStackHighWaterMark(handles[i])));
      strncat(stacks, item, sizeof(stacks) - strlen(stacks) - 1);
    }
    serialPrintf("%s", stacks);
  }
}

}  // namespace app
