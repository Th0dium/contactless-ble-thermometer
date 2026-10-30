#include "indicators.h"
#include "config.h"
#include <Wire.h>

void formatTempC(char *out, float tempC) {
    dtostrf(tempC, 1, 1, out);
    strcat(out, "C");
}

bool Indicators::begin() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    Wire.beginTransmission(I2C_ADDR_SSD1306);
    bool found = Wire.endTransmission() == 0;

    // U8x8 would switch the shared bus to 400 kHz; MLX90614 is rated 100 kHz max.
    _oled.setBusClock(100000);
    _oled.begin();
    _oled.setFont(u8x8_font_chroma48medium8_r);
    return found;
}

void Indicators::drawBle(bool bleConnected) {
    _oled.drawString(0, 6, bleConnected ? "BLE:OK" : "BLE:--");
}

void Indicators::showScreen(const char *bigLine, const char *smallLine,
                             float threshold, bool bleConnected) {
    char buf[17], t[10];
    _oled.clear();
    _oled.draw2x2String(0, 0, bigLine);
    _oled.drawString(0, 3, smallLine);

    formatTempC(t, threshold);
    snprintf(buf, sizeof(buf), "Thresh:%s", t);
    _oled.drawString(0, 5, buf);
    drawBle(bleConnected);

    _patternActive = false;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
}

void Indicators::showResult(ResultClass result, float tempC, float threshold, bool bleConnected,
                            const char *errText) {
    char big[10];
    const char *small;

    switch (result) {
        case ResultClass::NORMAL:
            formatTempC(big, tempC);
            small = "BINH THUONG";
            break;
        case ResultClass::ALARM:
            formatTempC(big, tempC);
            small = "CANH BAO: SOT";
            break;
        case ResultClass::SUSPECT:
            strcpy(big, "--.-C");
            small = "Yeu cau do lai";
            break;
        default: // ERROR
            strcpy(big, "LOI");
            small = errText;
            break;
    }
    showScreen(big, small, threshold, bleConnected);

    _activePattern = result;
    _patternStartMs = millis();
    _patternActive = true;

    if (result == ResultClass::ALARM) {
        tone(PIN_BUZZER, 2000); // 3 beeps/sec handled in tick() via on/off toggling
    } else if (result == ResultClass::ERROR) {
        tone(PIN_BUZZER, 1500, BUZZER_ERROR_DURATION_MS);
    }
}

void Indicators::tick() {
    if (!_patternActive) return;
    unsigned long elapsed = millis() - _patternStartMs;

    switch (_activePattern) {
        case ResultClass::NORMAL:
            digitalWrite(PIN_LED_GREEN, elapsed < LED_GREEN_ON_MS ? HIGH : LOW);
            if (elapsed >= LED_GREEN_ON_MS) _patternActive = false;
            break;

        case ResultClass::ALARM: {
            bool ledOn = elapsed < LED_RED_ON_MS;
            digitalWrite(PIN_LED_RED, ledOn ? HIGH : LOW);
            bool beepPhase = (elapsed % (1000 / BUZZER_ALARM_BEEP_HZ)) < (500 / BUZZER_ALARM_BEEP_HZ);
            if (elapsed < BUZZER_ALARM_DURATION_MS && beepPhase) {
                tone(PIN_BUZZER, 2000);
            } else {
                noTone(PIN_BUZZER);
            }
            if (elapsed >= LED_RED_ON_MS && elapsed >= BUZZER_ALARM_DURATION_MS) _patternActive = false;
            break;
        }

        case ResultClass::SUSPECT:
        case ResultClass::ERROR: {
            bool blinkOn = (elapsed % LED_BLINK_PERIOD_MS) < LED_BLINK_PERIOD_MS / 2; // 0.2 s period
            digitalWrite(PIN_LED_RED, blinkOn ? HIGH : LOW);
            // Blink pattern latches until the next measurement replaces it —
            // no natural "done" point per spec, so it doesn't self-clear.
            break;
        }
    }
}
