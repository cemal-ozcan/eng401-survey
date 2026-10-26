// Survey firmware entry point: initialises the HAL, creates queues and the
// FreeRTOS tasks (see tasks.h for the data flow), then deletes the Arduino
// loop task. No application code runs in loop().

#include <Arduino.h>

#include "build_info.h"
#include "config.h"
#include "hal.h"
#include "shared_state.h"
#include "tasks.h"

namespace app {

SharedState shared;
QueueHandle_t logQueue = nullptr;
QueueHandle_t netQueue = nullptr;
SemaphoreHandle_t serialMutex = nullptr;
std::atomic<uint32_t> queueDrops{0};
std::atomic<uint32_t> recordsProduced{0};
std::atomic<uint32_t> recordsLogged{0};
std::atomic<uint16_t> pendingMark{0};
std::atomic<uint64_t> lastLogUs{0};
std::atomic<bool> storageOk{false};

}  // namespace app

namespace {

TaskHandle_t handles[6];

void startTask(TaskFunction_t fn, const char* name, uint32_t stack, uint32_t prio, int core, int index,
               void* arg = nullptr) {
  TaskHandle_t h = nullptr;
  if (xTaskCreatePinnedToCore(fn, name, stack, arg, prio, &h, core) != pdPASS) {
    Serial.printf("# FATAL cannot create task %s\n", name);
  }
  if (index >= 0) handles[index] = h;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\n# ENG401 survey firmware v%s (%s sensors), sample period %lu ms\n", build_info::version(),
                build_info::modeName(), static_cast<unsigned long>(config::kSamplePeriodMs));

  app::serialMutex = xSemaphoreCreateMutex();
  app::shared.init();
  app::logQueue = xQueueCreate(config::kQueueLength, sizeof(record::Record));
  app::netQueue = xQueueCreate(config::kQueueLength, sizeof(record::Record));

  hal::Devices& d = hal::devices();
  Serial.printf("# INIT GNSS %s (baud %lu)\n", d.gnss.begin(config::kGnssBaud) ? "OK" : "FAIL",
                static_cast<unsigned long>(config::kGnssBaud));
  app::storageOk = d.storage.begin();
  Serial.printf("# INIT SD %s\n", app::storageOk ? "OK" : "FAIL");

  using namespace config;
  startTask(app::gnssTask, "gnss", kStackGnss, kPrioGnss, kAppCore, 0);
  startTask(app::sensorTask, "sensor", kStackSensor, kPrioSensor, kAppCore, 1);
  startTask(app::sampleTask, "sample", kStackSample, kPrioSample, kAppCore, 2);
  startTask(app::logTask, "log", kStackLog, kPrioLog, kAppCore, 3);
  startTask(app::netTask, "net", kStackNet, kPrioNet, kNetCore, 4);
  startTask(app::uiTask, "ui", kStackUi, kPrioUi, kAppCore, 5);
  startTask(app::monitorTask, "monitor", kStackMonitor, kPrioMonitor, kAppCore, -1, handles);
}

void loop() { vTaskDelete(nullptr); }  // everything runs in tasks
