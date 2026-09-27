#include "ble_protocol.h"
#include "config.h"
#include "crc8.h"

BleProtocol::BleProtocol(uint8_t rxPin, uint8_t txPin) : _serial(rxPin, txPin) {}

void BleProtocol::begin() {
    _serial.begin(BLE_BAUD_RATE);
}

const char *BleProtocol::statusText(ResultClass r) {
    switch (r) {
        case ResultClass::NORMAL:  return "NORMAL";
        case ResultClass::ALARM:   return "ALARM";
        case ResultClass::SUSPECT: return "SUSPECT";
        default:                   return "ERROR";
    }
}

void BleProtocol::sendTelemetry(float tempC, ResultClass result) {
    char payload[24];
    snprintf(payload, sizeof(payload), "TEMP,%.1f,%s", tempC, statusText(result));
    uint8_t sum = crc8((const uint8_t *)payload, strlen(payload));
    _serial.print(payload);
    _serial.print(',');
    if (sum < 0x10) _serial.print('0');
    _serial.print(sum, HEX);
    _serial.print("\r\n");
}

void BleProtocol::poll(CalibrationData &cal) {
    while (_serial.available()) {
        char c = _serial.read();
        if (c == '\n') {
            handleLine(cal);
            _len = 0;
            continue;
        }
        if (c == '\r') continue;
        if (_len < BLE_CMD_MAX_LEN) {
            _buf[_len++] = c;
        } else {
            _len = 0; // overflow: drop and resync on next line
        }
    }
}

void BleProtocol::handleLine(CalibrationData &cal) {
    _buf[_len] = '\0';

    if (strncmp(_buf, "SET_THRESH=", 11) == 0) {
        float v = atof(_buf + 11);
        if (v < THRESHOLD_MIN_C || v > THRESHOLD_MAX_C) {
            _serial.print("ERR:RANGE\r\n");
            return;
        }
        cal.threshold = v;
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

    } else {
        _serial.print("ERR:UNKNOWN\r\n");
    }
}
