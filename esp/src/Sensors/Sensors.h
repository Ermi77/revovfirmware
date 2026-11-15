#pragma once

#include <stdint.h>

namespace revov {

struct SensorData {
    float temperatureC = 0.0f;
    float voltageV = 0.0f;
    bool lowVoltage = false;
    bool sensorError = false;
    uint32_t lastUpdateMs = 0;
};

void sensors_init();
void sensors_update();
const SensorData& sensors_getData();
bool sensors_isLowVoltage();
float sensors_getTemperatureC();
float sensors_getVoltageV();

}  // namespace revov