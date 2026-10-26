#pragma once

// Application configuration. Change here, rebuild, re-flash.

#include <cstdint>

namespace config {

// Sampling: one record per period (handout "requested sampling interval").
constexpr uint32_t kSamplePeriodMs = 1000;

constexpr uint32_t kGnssBaud = 9600;       // u-blox default
constexpr uint32_t kGnssPollMs = 20;       // UART drain period
constexpr uint32_t kSensorLoopMs = 20;     // ToF poll period
constexpr uint32_t kImuEveryLoops = 5;     // 100 ms -> 10 Hz IMU, averaged per record
constexpr uint32_t kEnvEveryLoops = 50;    // 1 s
constexpr uint32_t kUiLoopMs = 10;
constexpr uint32_t kMonitorPeriodMs = 10000;

constexpr uint32_t kQueueLength = 20;      // records buffered per consumer

// FreeRTOS priorities (higher = more urgent). Sampling must never be late.
constexpr uint32_t kPrioSample = 5;
constexpr uint32_t kPrioGnss = 4;
constexpr uint32_t kPrioSensor = 4;
constexpr uint32_t kPrioLog = 2;
constexpr uint32_t kPrioNet = 2;
constexpr uint32_t kPrioUi = 1;
constexpr uint32_t kPrioMonitor = 1;

constexpr uint32_t kStackSample = 4096;
constexpr uint32_t kStackGnss = 4096;
constexpr uint32_t kStackSensor = 4096;
constexpr uint32_t kStackLog = 6144;
constexpr uint32_t kStackNet = 4096;
constexpr uint32_t kStackUi = 2048;
constexpr uint32_t kStackMonitor = 4096;

// Core 0 runs the Wi-Fi stack; acquisition stays on core 1.
constexpr int kAppCore = 1;
constexpr int kNetCore = 0;

}  // namespace config
