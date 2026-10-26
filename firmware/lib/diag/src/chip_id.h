#pragma once

// Identification of I2C devices by address and by their ID registers.
// Pure functions: the firmware reads the registers, this code interprets them.

#include <cstdint>

namespace diag {

// Human-readable guess for an address found by the I2C scan.
const char* nameForAddress(uint8_t addr);

// MPU-60x0 / 65x0 family, register 0x75 (WHO_AM_I).
constexpr uint8_t kMpuWhoAmIReg = 0x75;
const char* mpuName(uint8_t whoAmI);
bool isMpu6050(uint8_t whoAmI);

// Bosch BMx/BME family, register 0xD0 (chip id).
constexpr uint8_t kBoschChipIdReg = 0xD0;
const char* boschName(uint8_t chipId);
bool isBme280(uint8_t chipId);

// ST time-of-flight: VL53L0X answers 0xEE at 8-bit register 0xC0,
// VL53L1X answers 0xEACC at 16-bit register 0x010F.
constexpr uint8_t kVl53l0xIdReg = 0xC0;
constexpr uint16_t kVl53l1xIdReg = 0x010F;
const char* tofName(uint8_t l0xId, uint16_t l1xId);

}  // namespace diag
