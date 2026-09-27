#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <Arduino.h>
#include "mlx90614.h"
#include "eeprom_store.h"

enum class MeasureStatus { OK, SUSPECT_SPREAD, SENSOR_ERROR, ENV_INVALID };
enum class ResultClass { SUSPECT, NORMAL, ALARM, ERROR };

// Spec 5: 8 samples/400ms, trim min+max, average remaining 6, ambient-gated,
// reject on >1.0C spread. Returns false only for hard sensor faults.
bool takeMeasurement(MLX90614 &sensor, const CalibrationData &cal,
                      float &outBodyTempC, MeasureStatus &outStatus);

// Spec 6.2 classification table. status != OK short-circuits to the
// matching class without looking at bodyTempC.
ResultClass classify(float bodyTempC, float threshold, MeasureStatus status);

#endif // MEASUREMENT_H
