#include "mlx90614.h"
#include "config.h"
#include "crc8.h"
#include <Wire.h>

static const uint8_t CMD_TAMB = 0x06;
static const uint8_t CMD_TOBJ1 = 0x07;

MLX90614::MLX90614(uint8_t i2cAddr) : _addr(i2cAddr) {}

bool MLX90614::begin() {
    uint16_t raw;
    return readWord(CMD_TAMB, raw);
}

bool MLX90614::readWord(uint8_t command, uint16_t &outRaw) {
    for (uint8_t attempt = 0; attempt <= I2C_RETRY_MAX; attempt++) { // 1 read + up to 3 re-reads
        Wire.beginTransmission(_addr);
        Wire.write(command);
        if (Wire.endTransmission(false) != 0) continue; // repeated start, no stop

        if (Wire.requestFrom(_addr, (uint8_t)3) != 3) continue;
        uint8_t lsb = Wire.read();
        uint8_t msb = Wire.read();
        uint8_t pec = Wire.read();

        uint8_t frame[5] = {
            (uint8_t)(_addr << 1), command,
            (uint8_t)((_addr << 1) | 1), lsb, msb
        };
        if (crc8(frame, 5) != pec) continue;
        if (msb & 0x80) continue; // bit 15 = sensor error flag, value invalid

        outRaw = ((uint16_t)msb << 8) | lsb;
        return true;
    }
    return false;
}

float MLX90614::rawToCelsius(uint16_t raw) {
    return raw * 0.02f - 273.15f;
}

bool MLX90614::readObjectTempC(float &outTempC) {
    uint16_t raw;
    if (!readWord(CMD_TOBJ1, raw)) return false;
    outTempC = rawToCelsius(raw);
    return true;
}

bool MLX90614::readAmbientTempC(float &outTempC) {
    uint16_t raw;
    if (!readWord(CMD_TAMB, raw)) return false;
    outTempC = rawToCelsius(raw);
    return true;
}
