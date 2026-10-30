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
static bool holdHandled = false;

static unsigned long lastIdlePacketMs = 0;
static float lastTemp = 0;
static float lastAmbient = 0;
static bool calMode = false;
static bool envLocked = false; // ambient outside 16-35 C => measuring locked
static bool bleShown = false;

// Screen shown while no result is on display: locked > calibration > ready.
static void showIdleScreen() {
    if (envLocked) {
        char t[10], small[17];
        formatTempC(t, lastAmbient);
        snprintf(small, sizeof(small), "Moi truong %s", t);
        indicators.showScreen("KHOA DO", small, cal.threshold, ble.connected());
    } else if (calMode) {
        indicators.showScreen("CAL", "Hieu chuan", cal.threshold, ble.connected());
    } else {
        indicators.showScreen("SAN SANG", "Nhan nut de do", cal.threshold, ble.connected());
    }
}

// Spec 4: sensor, display, EEPROM, BLE — show ready or the failing codes.
static void runSelfTestAndReport(bool oledOk) {
    char codes[16] = "";
    if (!sensor.begin()) strcat(codes, " E1");
    if (!oledOk) strcat(codes, " E2");
    if (!eeprom_store::load(cal)) {
        strcat(codes, " E3");
        eeprom_store::save(cal); // defaults already loaded; store them so E3 doesn't repeat
    }
    if (!ble.probe()) strcat(codes, " E4");

    if (codes[0]) {
        indicators.showResult(ResultClass::ERROR, 0, cal.threshold, false, codes + 1);
    } else {
        showIdleScreen();
    }
}

static void setCalMode(bool on) {
    calMode = on;
    showIdleScreen();
}

// Locks/unlocks measuring as ambient leaves/re-enters the valid range.
static void checkAmbient() {
    float ambient;
    if (!sensor.readAmbientTempC(ambient)) return; // read faults surface as E1 on next measurement
    lastAmbient = ambient;
    bool outOfRange = ambient < AMBIENT_MIN_C || ambient > AMBIENT_MAX_C;
    if (outOfRange != envLocked) {
        envLocked = outOfRange;
        showIdleScreen();
    }
}

static void measureAndReport() {
    float skin, ambient;
    MeasureStatus status;
    takeMeasurement(sensor, skin, ambient, status);

    if (status == MeasureStatus::ENV_INVALID) {
        lastAmbient = ambient;
        envLocked = true;
        showIdleScreen();
        return;
    }

    if (calMode && status == MeasureStatus::OK) {
        // Raw skin temperature, no compensation: compare with a medical
        // thermometer, then send CALIBRATE=<k1>,<k2>.
        char big[10], t[10], small[17];
        formatTempC(big, skin);
        formatTempC(t, ambient);
        snprintf(small, sizeof(small), "CAL Ta:%s", t);
        indicators.showScreen(big, small, cal.threshold, ble.connected());
        ble.sendTelemetry(skin, ambient, "RAW");
        return;
    }

    float bodyTemp = status == MeasureStatus::OK ? compensate(skin, ambient, cal) : 0;
    ResultClass result = classify(bodyTemp, cal.threshold, status);
    const char *errText = status == MeasureStatus::SENSOR_ERROR ? "E1 CAM BIEN" : "E5 NGOAI DAI";
    indicators.showResult(result, bodyTemp, cal.threshold, ble.connected(), errText);
    ble.sendTelemetry(bodyTemp, ambient, BleProtocol::statusText(result));
    lastTemp = bodyTemp;
}

void setup() {
    // After a watchdog reset the watchdog stays enabled — turn it off before
    // the (slow) self-test, or the board would reset in a loop.
    MCUSR = 0;
    wdt_disable();

    Wire.begin();
    bool oledOk = indicators.begin();
    ble.begin();

    pinMode(PIN_BUTTON_FUNCTION, INPUT_PULLUP);
    pinMode(PIN_BUTTON_RESET, INPUT_PULLUP);

    runSelfTestAndReport(oledOk);

    wdt_enable(WDTO_2S);
}

void loop() {
    wdt_reset();

    // RESET button: restart via watchdog, setup() reruns the self-test.
    if (digitalRead(PIN_BUTTON_RESET) == LOW) {
        wdt_enable(WDTO_15MS);
        for (;;) {}
    }

    // Button: short press = measure now, hold 3s = toggle calibration mode.
    bool buttonDown = digitalRead(PIN_BUTTON_FUNCTION) == LOW;
    unsigned long heldMs = millis() - buttonPressStartMs;
    if (buttonDown && !buttonWasDown) {
        buttonPressStartMs = millis();
        holdHandled = false;
    } else if (!buttonDown && buttonWasDown) {
        if (!holdHandled && heldMs >= BUTTON_DEBOUNCE_MS) {
            measureRequested = true;
        }
    } else if (buttonDown && !holdHandled && heldMs >= BUTTON_HOLD_CALIBRATION_MS) {
        holdHandled = true; // releasing after the hold must not also measure
        setCalMode(!calMode);
    }
    buttonWasDown = buttonDown;

    bool wantCal = calMode;
    ble.poll(cal, wantCal);
    if (wantCal != calMode) setCalMode(wantCal);

    if (measureRequested) {
        measureRequested = false;
        if (envLocked) {
            showIdleScreen(); // measuring is locked, show the reason again
        } else {
            measureAndReport();
        }
        lastIdlePacketMs = millis();
    } else if (millis() - lastIdlePacketMs >= BLE_IDLE_PACKET_MS) {
        checkAmbient();
        ble.sendTelemetry(lastTemp, lastAmbient, calMode ? "CAL" : envLocked ? "LOCKED" : "IDLE");
        lastIdlePacketMs = millis();
    }

    if (ble.connected() != bleShown) {
        bleShown = ble.connected();
        indicators.drawBle(bleShown);
    }

    indicators.tick();
}
