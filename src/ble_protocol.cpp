#include "ble_protocol.h"
#include "config.h"
#include "crc8.h"

BleProtocol::BleProtocol(uint8_t rxPin, uint8_t txPin) : _serial(rxPin, txPin) {}

void BleProtocol::begin() {
    _serial.begin(BLE_BAUD_RATE);
}

// Collects the whole reply for 500 ms (so it doesn't leak into poll()) and
// checks that it starts with "OK".
bool BleProtocol::replyIsOk() {
    char reply[2] = {0, 0};
    uint8_t n = 0;
    unsigned long start = millis();
    while (millis() - start < 500) {
        if (_serial.available()) {
            char c = _serial.read();
            if (n < 2) reply[n++] = c;
        }
    }
    return reply[0] == 'O' && reply[1] == 'K';
}

bool BleProtocol::probe() {
    // Genuine HM-10 wants bare "AT"; common clones (CC41-A, MLT-BT05) need
    // CRLF. Retry a few times: the module may still be booting at power-up.
    const char *eol = nullptr;
    for (uint8_t i = 0; i < 3 && !eol; i++) {
        _serial.print("AT");
        if (replyIsOk()) { eol = ""; break; }
        _serial.print("AT\r\n");
        if (replyIsOk()) eol = "\r\n";
    }
    if (!eol) return false;
    _serial.print("AT+NOTI1");
    _serial.print(eol);
    replyIsOk();
    return true;
}

const char *BleProtocol::statusText(ResultClass r) {
    switch (r) {
        case ResultClass::NORMAL:  return "NORMAL";
        case ResultClass::ALARM:   return "ALARM";
        case ResultClass::SUSPECT: return "SUSPECT";
        default:                   return "ERROR";
    }
}

void BleProtocol::sendTelemetry(float tempC, float ambientC, const char *status) {
    char payload[32], t[8], a[8];
    dtostrf(tempC, 1, 1, t);
    dtostrf(ambientC, 1, 1, a);
    snprintf(payload, sizeof(payload), "TEMP,%s,%s,%s", t, a, status);
    uint8_t sum = crc8((const uint8_t *)payload, strlen(payload));
    _serial.print(payload);
    _serial.print(',');
    if (sum < 0x10) _serial.print('0');
    _serial.print(sum, HEX);
    _serial.print("\r\n");
}

void BleProtocol::poll(CalibrationData &cal, bool &calMode) {
    while (_serial.available()) {
        char c = _serial.read();
        if (c == '\n') {
            handleLine(cal, calMode);
            _len = 0;
            continue;
        }
        if (c == '\r') continue;
        if (_len < BLE_CMD_MAX_LEN) {
            _buf[_len++] = c;
        } else {
            _len = 0; // overflow: drop and resync on next line
        }
        // HM-10 connection notifications (enabled in probe()) have no CRLF.
        if (_len == 7 && strncmp(_buf, "OK+CONN", 7) == 0) {
            _connected = true;
            _len = 0;
        } else if (_len == 7 && strncmp(_buf, "OK+LOST", 7) == 0) {
            _connected = false;
            _len = 0;
        }
    }
}

void BleProtocol::handleLine(CalibrationData &cal, bool &calMode) {
    _buf[_len] = '\0';
    _connected = true; // a command can only come from a connected phone

    if (strncmp(_buf, "SET_THRESH=", 11) == 0) {
        float v = atof(_buf + 11);
        if (v < THRESHOLD_MIN_C || v > THRESHOLD_MAX_C) {
            _serial.print("ERR:RANGE\r\n");
            return;
        }
        cal.threshold = round(v * 10) / 10.0f; // spec: 0.1 C steps
        eeprom_store::save(cal);
        _serial.print("OK\r\n");

    } else if (strcmp(_buf, "GET_STATUS") == 0) {
        _serial.print("OK\r\n");

    } else if (strncmp(_buf, "CALIBRATE=", 10) == 0) {
        char *comma = strchr(_buf + 10, ',');
        if (!comma) {
            _serial.print("ERR:FORMAT\r\n");
            return;
        }
        float k1 = atof(_buf + 10);
        float k2 = atof(comma + 1);
        if (k1 < CALIBRATION_K1_MIN || k1 > CALIBRATION_K1_MAX ||
            k2 < CALIBRATION_K2_MIN || k2 > CALIBRATION_K2_MAX) {
            _serial.print("ERR:RANGE\r\n");
            return;
        }
        cal.k1 = k1;
        cal.k2 = k2;
        eeprom_store::save(cal);
        _serial.print("OK\r\n");

    } else if (strcmp(_buf, "CAL_MODE=1") == 0 || strcmp(_buf, "CAL_MODE=0") == 0) {
        calMode = _buf[9] == '1';
        _serial.print("OK\r\n");

    } else {
        _serial.print("ERR:UNKNOWN\r\n");
    }
}
