#include "Sensors.h"

#include "../Config.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace revov {
namespace {

SensorData sensorData;

constexpr float VOLTAGE_DIVIDER_RATIO = 5.0f;
constexpr float ADC_MAX_VOLTAGE = 3.3f;
constexpr int ADC_MAX_VALUE = 4095;
constexpr float LOW_VOLTAGE_THRESHOLD = 10.0f;

constexpr int SAMPLE_COUNT = 10;
constexpr uint32_t UPDATE_INTERVAL_MS = 1000;

uint32_t lastUpdate = 0;
float voltageSamples[SAMPLE_COUNT] = {};
float tempSamples[SAMPLE_COUNT] = {};
int sampleIndex = 0;
bool samplesFilled = false;

float readVoltage() {
#ifdef ARDUINO
    int raw = analogRead(PIN_VOLTAGE_SENSOR);
    float adcVoltage = (raw / static_cast<float>(ADC_MAX_VALUE)) * ADC_MAX_VOLTAGE;
    return adcVoltage * VOLTAGE_DIVIDER_RATIO;
#else
    return 12.0f;
#endif
}

float readTemperature() {
#ifdef ARDUINO
    int raw = analogRead(PIN_TEMP_SENSOR);
    float voltage = (raw / static_cast<float>(ADC_MAX_VALUE)) * ADC_MAX_VOLTAGE;
    float tempC = (voltage - 0.5f) * 100.0f;
    return tempC;
#else
    return 25.0f;
#endif
}

float average(float* samples, int count) {
    float sum = 0.0f;
    for (int i = 0; i < count; ++i) sum += samples[i];
    return sum / count;
}

}  // namespace

void sensors_init() {
#ifdef ARDUINO
    pinMode(PIN_VOLTAGE_SENSOR, INPUT);
    pinMode(PIN_TEMP_SENSOR, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
#endif
    sensorData.temperatureC = readTemperature();
    sensorData.voltageV = readVoltage();
    sensorData.lowVoltage = sensorData.voltageV < LOW_VOLTAGE_THRESHOLD;
    sensorData.lastUpdateMs = millis();
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        voltageSamples[i] = sensorData.voltageV;
        tempSamples[i] = sensorData.temperatureC;
    }
    samplesFilled = true;
}

void sensors_update() {
    uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastUpdate) < UPDATE_INTERVAL_MS) return;
    lastUpdate = now;

    float v = readVoltage();
    float t = readTemperature();

    voltageSamples[sampleIndex] = v;
    tempSamples[sampleIndex] = t;
    sampleIndex = (sampleIndex + 1) % SAMPLE_COUNT;
    if (sampleIndex == 0) samplesFilled = true;

    int count = samplesFilled ? SAMPLE_COUNT : sampleIndex;
    sensorData.voltageV = average(voltageSamples, count);
    sensorData.temperatureC = average(tempSamples, count);
    sensorData.lowVoltage = sensorData.voltageV < LOW_VOLTAGE_THRESHOLD;
    sensorData.lastUpdateMs = now;
}

const SensorData& sensors_getData() {
    return sensorData;
}

bool sensors_isLowVoltage() {
    return sensorData.lowVoltage;
}

float sensors_getTemperatureC() {
    return sensorData.temperatureC;
}

float sensors_getVoltageV() {
    return sensorData.voltageV;
}

}  // namespace revov