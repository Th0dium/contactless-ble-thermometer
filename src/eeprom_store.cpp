#include "eeprom_store.h"
#include "config.h"
#include "crc8.h"
#include <EEPROM.h>

static uint8_t computeCrc(const CalibrationData &d) {
    uint8_t buf[EEPROM_ADDR_CRC8];
    buf[0] = EEPROM_MAGIC;
    memcpy(buf + 1, &d.threshold, sizeof(float));
    memcpy(buf + 5, &d.k1, sizeof(float));
    memcpy(buf + 9, &d.k2, sizeof(float));
    return crc8(buf, EEPROM_ADDR_CRC8);
}

bool eeprom_store::load(CalibrationData &outData) {
    uint8_t magic = EEPROM.read(EEPROM_ADDR_MAGIC);
    EEPROM.get(EEPROM_ADDR_THRESHOLD, outData.threshold);
    EEPROM.get(EEPROM_ADDR_K1, outData.k1);
    EEPROM.get(EEPROM_ADDR_K2, outData.k2);
    uint8_t storedCrc = EEPROM.read(EEPROM_ADDR_CRC8);

    bool valid = (magic == EEPROM_MAGIC) && (computeCrc(outData) == storedCrc);
    if (!valid) {
        outData.threshold = THRESHOLD_DEFAULT_C;
        outData.k1 = CALIBRATION_K1_DEFAULT;
        outData.k2 = CALIBRATION_K2_DEFAULT;
    }
    return valid;
}

void eeprom_store::save(const CalibrationData &data) {
    EEPROM.update(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.put(EEPROM_ADDR_THRESHOLD, data.threshold);
    EEPROM.put(EEPROM_ADDR_K1, data.k1);
    EEPROM.put(EEPROM_ADDR_K2, data.k2);
    EEPROM.update(EEPROM_ADDR_CRC8, computeCrc(data));
}
