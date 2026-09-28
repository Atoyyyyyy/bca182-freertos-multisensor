#ifndef APP_H
#define APP_H

#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include "system_types.h"

// ============================================================
// SENSOR DATA
// ============================================================

struct SensorData
{
    float temperature;
    float humidity;
    int lightLevel;
    bool motionDetected;
};

// ============================================================
// RTOS OBJECTS
// ============================================================

extern QueueHandle_t sensorQueue;
extern QueueHandle_t alarmQueue;

extern EventGroupHandle_t systemEvents;

// ============================================================
// SYSTEM STATE
// ============================================================

extern volatile SystemState currentSystemState;

constexpr TickType_t INACTIVITY_TIMEOUT =
    pdMS_TO_TICKS(15000);

// ============================================================
// EVENT BITS
// ============================================================

#define EVENT_MOTION  (1U << 0)
#define EVENT_ACTIVE  (1U << 1)

// ============================================================
// DISPLAY STATE
// ============================================================

extern volatile DisplayMode currentDisplayMode;

// ============================================================
// TEMPERATURE ALARM LOGIC
// ============================================================

AlarmState evaluateTemperature(float temperature);

#endif