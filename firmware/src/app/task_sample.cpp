// The sampling task: the heart of the real-time design. Runs at a fixed period
// with the highest application priority, stamps each record with esp_timer time
// and UTC from the common time base, and hands it to the consumers via queues.
// It never blocks: queue sends use a zero timeout and count drops instead.

#include <Arduino.h>

#include "build_info.h"
#include "config.h"
#include "record_builder.h"
#include "shared_state.h"
#include "tasks.h"

namespace app {

void sampleTask(void*) {
  hal::Devices& d = hal::devices();
  const TickType_t period = pdMS_TO_TICKS(config::kSamplePeriodMs);
  uint32_t seq = 0;
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWake, period);
    // If we were late by more than a period, re-sync instead of firing a burst
    // of catch-up ticks; the gap is counted as missed samples.
    if (xTaskGetTickCount() - lastWake > period) lastWake = xTaskGetTickCount();

    const uint64_t now = d.clock.nowUs();
    uint32_t missed = 0;
    shared.tickStats(now, missed);

    record::Record r;
    r.seq = seq++;
    r.tUs = now;
    r.utcMs = shared.utcAt(now);
    if (r.utcMs >= 0) r.flags |= record::kFlagUtcSynced;
    if (build_info::isMock()) r.flags |= record::kFlagMock;
    if (storageOk.load()) r.flags |= record::kFlagSdOk;

    nmea::GnssData g;
    uint64_t gnssUs = 0;
    shared.getGnss(g, gnssUs);
    record::fillGnss(r, g, gnssUs, now);

    SensorSnapshot s = shared.takeSensors();
    record::fillImu(r, s.imuOk ? &s.imu : nullptr);
    record::fillEnv(r, s.envOk ? &s.env : nullptr);
    record::fillRange(r, s.rangeOk ? &s.range : nullptr);

    r.missed = missed;
    r.mark = pendingMark.exchange(0);

    if (xQueueSend(logQueue, &r, 0) != pdTRUE) ++queueDrops;
    if (xQueueSend(netQueue, &r, 0) != pdTRUE) ++queueDrops;
    ++recordsProduced;
  }
}

}  // namespace app
