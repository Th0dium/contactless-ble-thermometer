#include "indicators.h"
#include "config.h"

void Indicators::begin() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    _oled.begin();
    _oled.setFont(u8x8_font_chroma48medium8_r);
}

void Indicators::drawScreen(const char *bigLine, const char *smallLine,
                             float threshold, bool bleConnected) {
    char buf[17];
    _oled.clear();
    _oled.draw2x2String(0, 0, bigLine);
    _oled.drawString(0, 3, smallLine);

    snprintf(buf, sizeof(buf), "Thresh:%.1fC", threshold);
    _oled.drawString(0, 5, buf);
    _oled.drawString(0, 6, bleConnected ? "BLE:OK" : "BLE:--");
}

void Indicators::showResult(ResultClass result, float tempC, float threshold, bool bleConnected) {
    char big[8];
    const char *small;

    switch (result) {
        case ResultClass::NORMAL:
            snprintf(big, sizeof(big), "%.1fC", tempC);
            small = "BINH THUONG";
            break;
        case ResultClass::ALARM:
            snprintf(big, sizeof(big), "%.1fC", tempC);
            small = "CANH BAO: SOT";
            break;
        case ResultClass::SUSPECT:
            strcpy(big, "--.-C");
            small = "Yeu cau do lai";
            break;
        default: // ERROR
            strcpy(big, "ERR");
            small = "Loi cam bien";
            break;
    }
    drawScreen(big, small, threshold, bleConnected);

    _activePattern = result;
    _patternStartMs = millis();
    _patternActive = true;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);

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
            bool blinkOn = (elapsed % (2 * LED_BLINK_PERIOD_MS)) < LED_BLINK_PERIOD_MS;
            digitalWrite(PIN_LED_RED, blinkOn ? HIGH : LOW);
            // Blink pattern latches until the next measurement replaces it —
            // no natural "done" point per spec, so it doesn't self-clear.
            break;
        }
    }
}
