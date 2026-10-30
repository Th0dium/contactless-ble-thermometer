#include "measurement.h"
#include "config.h"

bool takeMeasurement(MLX90614 &sensor, float &outSkinC, float &outAmbientC,
                      MeasureStatus &outStatus) {
    if (!sensor.readAmbientTempC(outAmbientC)) {
        outStatus = MeasureStatus::SENSOR_ERROR;
        return false;
    }
    if (outAmbientC < AMBIENT_MIN_C || outAmbientC > AMBIENT_MAX_C) {
        outStatus = MeasureStatus::ENV_INVALID;
        return false;
    }

    float samples[SAMPLE_COUNT];
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        if (!sensor.readObjectTempC(samples[i])) {
            outStatus = MeasureStatus::SENSOR_ERROR;
            return false;
        }
        if (i + 1 < SAMPLE_COUNT) delay(SAMPLE_INTERVAL_MS);
    }

    // Insertion sort — 8 elements, cheaper than pulling in <algorithm>.
    for (uint8_t i = 1; i < SAMPLE_COUNT; i++) {
        float key = samples[i];
        int8_t j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }

    float spread = samples[SAMPLE_COUNT - 1] - samples[0];
    if (spread > MAX_SAMPLE_SPREAD_C) {
        outStatus = MeasureStatus::SUSPECT_SPREAD;
        return false;
    }

    float sum = 0;
    for (uint8_t i = 1; i < SAMPLE_COUNT - 1; i++) sum += samples[i];
    outSkinC = sum / TRIMMED_COUNT;
    outStatus = MeasureStatus::OK;
    return true;
}

float compensate(float skinC, float ambientC, const CalibrationData &cal) {
    return skinC + cal.k1 * (skinC - ambientC) + cal.k2;
}

ResultClass classify(float bodyTempC, float threshold, MeasureStatus status) {
    if (status == MeasureStatus::SUSPECT_SPREAD) return ResultClass::SUSPECT;
    if (status == MeasureStatus::SENSOR_ERROR || status == MeasureStatus::ENV_INVALID)
        return ResultClass::ERROR;

    if (bodyTempC < TEMP_SUSPECT_LOW_C) return ResultClass::SUSPECT;
    if (bodyTempC > TEMP_RANGE_MAX_C) return ResultClass::ERROR;
    if (bodyTempC >= threshold) return ResultClass::ALARM;
    return ResultClass::NORMAL;
}
