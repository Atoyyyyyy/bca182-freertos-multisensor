#ifndef RTOS_OBJECTS_H
#define RTOS_OBJECTS_H

#include "app.h"

extern QueueHandle_t sensorQueue;
extern QueueHandle_t alarmQueue;

extern EventGroupHandle_t systemEvents;

extern volatile SystemState currentSystemState;

extern volatile DisplayMode currentDisplayMode;

#endif