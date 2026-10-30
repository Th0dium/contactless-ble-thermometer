#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"
#include "eeprom_store.h"
#include "measurement.h"

// Command protocol is NOT specified in the requirements doc (no command
// table given) — this is a proposed minimal ASCII line protocol for the
// starter build, easy to swap later once the app side is defined:
//   SET_THRESH=37.8\r\n     -> OK\r\n | ERR:<reason>\r\n
//   GET_STATUS\r\n          -> OK\r\n (status is pushed via TEMP,... packets)
//   CALIBRATE=<k1>,<k2>\r\n -> OK\r\n | ERR:<reason>\r\n
//   CAL_MODE=1 / CAL_MODE=0 -> OK\r\n (enter / leave calibration mode)
// Outgoing telemetry: TEMP,<value>,<ambient>,<statusText>,<crc8-hex>\r\n
class BleProtocol {
public:
    BleProtocol(uint8_t rxPin, uint8_t txPin);
    void begin();

    // Self-test: HM-10 answers "AT" with "OK" (only while no phone is
    // connected). Also enables OK+CONN/OK+LOST notifications.
    bool probe();

    // Reads any pending bytes, executes a complete command line if one
    // arrived. Mutates cal and persists to EEPROM on SET_THRESH/CALIBRATE,
    // sets calMode on CAL_MODE.
    void poll(CalibrationData &cal, bool &calMode);

    bool connected() const { return _connected; }

    // Sends one telemetry packet now (new result or idle heartbeat).
    void sendTelemetry(float tempC, float ambientC, const char *status);

    static const char *statusText(ResultClass r);

private:
    SoftwareSerial _serial;
    char _buf[BLE_CMD_MAX_LEN + 1];
    uint8_t _len = 0;
    bool _connected = false;

    bool replyIsOk();
    void handleLine(CalibrationData &cal, bool &calMode);
};

#endif // BLE_PROTOCOL_H
