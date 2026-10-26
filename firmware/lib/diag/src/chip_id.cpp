#include "chip_id.h"

namespace diag {

const char* nameForAddress(uint8_t addr) {
  switch (addr) {
    case 0x68: return "MPU6050 (AD0=GND)";
    case 0x69: return "MPU6050 (AD0=3V3)";
    case 0x76: return "BME280 (SDO=GND)";
    case 0x77: return "BME280 (SDO=3V3)";
    case 0x29: return "VL53L0X / VL53L1X";
    case 0x0D: return "QMC5883L compass (GNSS module?)";
    case 0x1E: return "HMC5883L compass (GNSS module?)";
    case 0x42: return "u-blox GNSS (I2C/DDC port)";
    default: return "unknown device";
  }
}

const char* mpuName(uint8_t id) {
  switch (id) {
    case 0x68: return "MPU6050";
    case 0x70: return "MPU6500";
    case 0x71: return "MPU9250";
    case 0x73: return "MPU9255";
    default: return "unknown MPU-like chip";
  }
}

bool isMpu6050(uint8_t id) { return id == 0x68; }

const char* boschName(uint8_t id) {
  switch (id) {
    case 0x60: return "BME280";
    case 0x56:
    case 0x57:
    case 0x58: return "BMP280 (no humidity!)";
    case 0x55: return "BMP180";
    case 0x61: return "BME680";
    default: return "unknown Bosch-like chip";
  }
}

bool isBme280(uint8_t id) { return id == 0x60; }

const char* tofName(uint8_t l0xId, uint16_t l1xId) {
  if (l0xId == 0xEE) return "VL53L0X";
  if (l1xId == 0xEACC) return "VL53L1X";
  return "unknown ToF chip";
}

}  // namespace diag
