#ifndef INDICATORS_H
#define INDICATORS_H

#include <Arduino.h>
#include <U8x8lib.h>
#include "measurement.h"

// Writes e.g. "37.5C" into out (needs >= 8 bytes). AVR snprintf has no %f.
void formatTempC(char *out, float tempC);

// Owns LED/buzzer/OLED. showResult() latches a non-blocking timed pattern;
// tick() must be called every loop iteration (no delay() anywhere here).
class Indicators {
public:
    // Returns false if the OLED doesn't answer on I2C (self-test item).
    bool begin();
    // errText is the small line shown for ResultClass::ERROR (error code).
    void showResult(ResultClass result, float tempC, float threshold, bool bleConnected,
                    const char *errText);
    // Plain screen (ready / locked / calibration); stops any LED/buzzer pattern.
    void showScreen(const char *bigLine, const char *smallLine,
                    float threshold, bool bleConnected);
    void drawBle(bool bleConnected);
    void tick();

private:
    U8X8_SSD1306_128X64_NONAME_HW_I2C _oled{U8X8_PIN_NONE};
    unsigned long _patternStartMs = 0;
    ResultClass _activePattern = ResultClass::NORMAL;
    bool _patternActive = false;
};

#endif // INDICATORS_H
