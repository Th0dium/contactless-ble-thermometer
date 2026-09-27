#ifndef MLX90614_H
#define MLX90614_H

#include <Arduino.h>

// Minimal MLX90614 driver: reads object (forehead) and ambient temperature
// over I2C, validating the sensor's PEC (CRC-8/MAXIM) byte and retrying on
// failure, per spec section 3.1 / 5.

class MLX90614 {
public:
    explicit MLX90614(uint8_t i2cAddr);

    // Probes the sensor (used by startup self-test).
    bool begin();

    // Reads object/ambient temperature in Celsius. Retries up to
    // I2C_RETRY_MAX times on PEC mismatch or bus error.
    // Returns false if all retries fail.
    bool readObjectTempC(float &outTempC);
    bool readAmbientTempC(float &outTempC);

private:
    uint8_t _addr;

    bool readWord(uint8_t command, uint16_t &outRaw);
    static float rawToCelsius(uint16_t raw);
};

#endif // MLX90614_H
