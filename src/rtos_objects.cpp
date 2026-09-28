#include "app.h"

// ============================================================
// RTOS OBJECTS
// ============================================================

QueueHandle_t sensorQueue =
    nullptr;

QueueHandle_t alarmQueue =
    nullptr;

EventGroupHandle_t systemEvents =
    nullptr;

// ============================================================
// SHARED SYSTEM STATE
// ============================================================

volatile SystemState currentSystemState =
    SystemState::ACTIVE;

volatile DisplayMode currentDisplayMode =
    DisplayMode::TEMPERATURE;