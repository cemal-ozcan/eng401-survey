#pragma once

// Pin assignment for ESP32 DevKit V1 (30 pin). Must match docs/PINOUT.md.

#include <cstdint>

namespace pins {

// I2C bus shared by MPU6050, BME280, VL53L0X
constexpr uint8_t kI2cSda = 21;
constexpr uint8_t kI2cScl = 22;
constexpr uint8_t kTofXshut = 33;

// GNSS on UART2 (ESP RX <- GNSS TX)
constexpr uint8_t kGnssRx = 16;
constexpr uint8_t kGnssTx = 17;

// microSD on VSPI
constexpr uint8_t kSdCs = 5;
constexpr uint8_t kSdSck = 18;
constexpr uint8_t kSdMiso = 19;
constexpr uint8_t kSdMosi = 23;

// Status LEDs (active HIGH through 330 ohm) and marker button (to GND)
constexpr uint8_t kLedFix = 25;    // green
constexpr uint8_t kLedLog = 26;    // blue
constexpr uint8_t kLedError = 27;  // red
constexpr uint8_t kButton = 32;

}  // namespace pins
