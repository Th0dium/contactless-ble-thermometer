#ifndef CRC8_H
#define CRC8_H

#include <Arduino.h>

// CRC-8/SMBus (polynomial 0x07, init 0x00) — the algorithm of the MLX90614
// PEC byte, reused here for EEPROM and BLE frame integrity.
uint8_t crc8(const uint8_t *data, size_t len);

#endif // CRC8_H
