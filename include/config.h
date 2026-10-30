#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin mapping (ATmega328P / Arduino Uno-compatible board)
// ---------------------------------------------------------------------------
// I2C (A4=SDA, A5=SCL) shared by MLX90614 and SSD1306 — hardware pins, not
// user-configurable, listed here only for reference.
// PB0/PB1/PB3/PB4 are fixed by the requirements doc (3.1/3.2); the rest
// were moved off those pins to avoid conflicts.
#define PIN_BLE_RX            2   // Arduino RX  <- HM-10 TX
#define PIN_BLE_TX            3   // Arduino TX  -> HM-10 RX
#define PIN_BUTTON_FUNCTION   8   // PB0, active LOW, internal pull-up
#define PIN_BUTTON_RESET      9   // PB1, active LOW, internal pull-up (soft reset trigger)
#define PIN_BUZZER            10  // PB2
#define PIN_LED_RED           11  // PB3
#define PIN_LED_GREEN         12  // PB4

// ---------------------------------------------------------------------------
// I2C addresses
// ---------------------------------------------------------------------------
#define I2C_ADDR_MLX90614     0x5A
#define I2C_ADDR_SSD1306      0x3C

// ---------------------------------------------------------------------------
// BLE UART (HM-10)
// ---------------------------------------------------------------------------
#define BLE_BAUD_RATE         9600
#define BLE_CMD_MAX_LEN       32   // spec 3.1: command strings, max 32 bytes, end CRLF

// ---------------------------------------------------------------------------
// Sampling / filtering (spec section 5)
// ---------------------------------------------------------------------------
#define SAMPLE_COUNT               8      // 8 samples in 400 ms
#define SAMPLE_WINDOW_MS            400
#define SAMPLE_INTERVAL_MS          (SAMPLE_WINDOW_MS / SAMPLE_COUNT) // 50 ms
#define TRIMMED_COUNT               6      // after dropping min & max
#define MAX_SAMPLE_SPREAD_C          1.0   // reject if (max-min) > 1.0 C
#define I2C_RETRY_MAX                3      // re-reads on PEC failure (1 + 3 attempts)

// ---------------------------------------------------------------------------
// Measurement classification (spec section 6.2)
// ---------------------------------------------------------------------------
#define TEMP_RANGE_MIN_C            32.0
#define TEMP_RANGE_MAX_C            42.5
#define TEMP_SUSPECT_LOW_C          35.0    // T < 35.0 => suspect / re-measure

#define THRESHOLD_MIN_C             35.0
#define THRESHOLD_MAX_C             40.0
#define THRESHOLD_STEP_C            0.1
#define THRESHOLD_DEFAULT_C         37.5

// ---------------------------------------------------------------------------
// Environment validity (spec section 5)
// ---------------------------------------------------------------------------
#define AMBIENT_MIN_C                16.0
#define AMBIENT_MAX_C                35.0

// ---------------------------------------------------------------------------
// Timing (spec section 6.1)
// ---------------------------------------------------------------------------
#define MEASURE_TIMEOUT_MS           1000   // whole measurement must finish within 1 s
#define STARTUP_TIMEOUT_MS           60000  // boot + thermal settle budget
#define BLE_IDLE_PACKET_MS           1000   // 1 packet/second while idle
#define ALARM_LATENCY_MAX_MS         200

#define LED_GREEN_ON_MS               3000
#define LED_RED_ON_MS                 5000
#define LED_BLINK_PERIOD_MS           200    // error / suspect blink period
#define BUZZER_ALARM_BEEP_HZ          3      // 3 beeps/second
#define BUZZER_ALARM_DURATION_MS      5000
#define BUZZER_ERROR_DURATION_MS      1000

#define BUTTON_DEBOUNCE_MS            50     // shorter presses are contact bounce
#define BUTTON_HOLD_CALIBRATION_MS    3000   // hold 3s => calibration mode
#define WATCHDOG_TIMEOUT_MS           2000   // main loop stall => reset

// ---------------------------------------------------------------------------
// EEPROM layout (with CRC8 protection, spec section 3.1/5)
// ---------------------------------------------------------------------------
#define EEPROM_MAGIC                 0xA6   // bumped: K1 meaning changed
#define EEPROM_ADDR_MAGIC            0
#define EEPROM_ADDR_THRESHOLD        1   // float, 4 bytes
#define EEPROM_ADDR_K1               5   // float, 4 bytes (calibration gain)
#define EEPROM_ADDR_K2               9   // float, 4 bytes (calibration offset)
#define EEPROM_ADDR_CRC8             13  // 1 byte
#define EEPROM_LAYOUT_SIZE           14

// Skin->core compensation: Tbody = Tobj + K1*(Tobj - Tamb) + K2.
// Defaults = no compensation; must be calibrated on real hardware.
#define CALIBRATION_K1_DEFAULT       0.0
#define CALIBRATION_K2_DEFAULT       0.0
#define CALIBRATION_K1_MIN           0.0
#define CALIBRATION_K1_MAX           1.0
#define CALIBRATION_K2_MIN           -5.0
#define CALIBRATION_K2_MAX           5.0

#endif // CONFIG_H
