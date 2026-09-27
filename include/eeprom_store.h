#ifndef EEPROM_STORE_H
#define EEPROM_STORE_H

#include <Arduino.h>

struct CalibrationData {
    float threshold;
    float k1;
    float k2;
};

namespace eeprom_store {
    // Loads calibration/threshold from EEPROM. Returns false (and fills
    // outData with spec defaults) if magic byte or CRC8 don't match —
    // caller is responsible for reporting the error per spec 3.1.
    bool load(CalibrationData &outData);

    // Writes only the bytes that actually changed (EEPROM.put on AVR core
    // already does byte-level compare-before-write), per spec 3.2.
    void save(const CalibrationData &data);
}

#endif // EEPROM_STORE_H
