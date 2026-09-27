#ifndef INDICATORS_H
#define INDICATORS_H

#include <Arduino.h>
#include <U8x8lib.h>
#include "measurement.h"

// Owns LED/buzzer/OLED. showResult() latches a non-blocking timed pattern;
// tick() must be called every loop iteration (no delay() anywhere here).
class Indicators {
public:
    void begin();
    void showResult(ResultClass result, float tempC, float threshold, bool bleConnected);
    void tick();

private:
    U8X8_SSD1306_128X64_NONAME_HW_I2C _oled{U8X8_PIN_NONE};
    unsigned long _patternStartMs = 0;
    ResultClass _activePattern = ResultClass::NORMAL;
    bool _patternActive = false;

    void drawScreen(const char *bigLine, const char *smallLine,
                    float threshold, bool bleConnected);
};

#endif // INDICATORS_H
