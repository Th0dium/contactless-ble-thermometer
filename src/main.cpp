#include <Arduino.h>
#include <Wire.h>
#include <avr/wdt.h>
#include "config.h"
#include "mlx90614.h"
#include "eeprom_store.h"
#include "measurement.h"
#include "indicators.h"
#include "ble_protocol.h"

static MLX90614 sensor(I2C_ADDR_MLX90614);
static Indicators indicators;
static BleProtocol ble(PIN_BLE_RX, PIN_BLE_TX);
static CalibrationData cal;

static volatile bool measureRequested = false;
static unsigned long buttonPressStartMs = 0;
static bool buttonWasDown = false;

static unsigned long lastIdlePacketMs = 0;

static void runSelfTestAndReport() {
    bool sensorOk = sensor.begin();
    bool eepromOk = eeprom_store::load(cal);

    if (!sensorOk) {
        indicators.showResult(ResultClass::ERROR, 0, cal.threshold, false);
    } else {
        indicators.showResult(ResultClass::SUSPECT, 0, cal.threshold, false); // "ready" screen reuses suspect's blank display
    }
    // eepromOk == false just means defaults were loaded; already handled by
    // eeprom_store::load() per spec 3.1 — not a hard boot failure.
    (void)eepromOk;
}

void setup() {
    Wire.begin();
    indicators.begin();
    ble.begin();

    pinMode(PIN_BUTTON_FUNCTION, INPUT_PULLUP);
    pinMode(PIN_BUTTON_RESET, INPUT_PULLUP);

    runSelfTestAndReport();

    wdt_enable(WDTO_2S);
}

void loop() {
    wdt_reset();

    // Button: short press = measure now, hold 3s = recalibrate to defaults.
    bool buttonDown = digitalRead(PIN_BUTTON_FUNCTION) == LOW;
    if (buttonDown && !buttonWasDown) {
        buttonPressStartMs = millis();
    } else if (!buttonDown && buttonWasDown) {
        if (millis() - buttonPressStartMs < BUTTON_HOLD_CALIBRATION_MS) {
            measureRequested = true;
        }
    } else if (buttonDown && millis() - buttonPressStartMs >= BUTTON_HOLD_CALIBRATION_MS) {
        cal.k1 = CALIBRATION_K1_DEFAULT;
        cal.k2 = CALIBRATION_K2_DEFAULT;
        eeprom_store::save(cal);
        buttonPressStartMs = millis(); // avoid re-trigger while still held
    }
    buttonWasDown = buttonDown;

    ble.poll(cal);

    if (measureRequested) {
        measureRequested = false;
        float bodyTemp;
        MeasureStatus status;
        takeMeasurement(sensor, cal, bodyTemp, status);
        ResultClass result = classify(bodyTemp, cal.threshold, status);
        indicators.showResult(result, bodyTemp, cal.threshold, true);
        ble.sendTelemetry(bodyTemp, result);
        lastIdlePacketMs = millis();
    } else if (millis() - lastIdlePacketMs >= BLE_IDLE_PACKET_MS) {
        ble.sendTelemetry(0, ResultClass::NORMAL); // idle heartbeat, no new reading
        lastIdlePacketMs = millis();
    }

    indicators.tick();
}
