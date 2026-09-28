#ifndef OLED_H
#define OLED_H

#include <stdbool.h>

bool OLED_Init();

bool OLED_Update();

void OLED_ClearBuffer();

void OLED_ShowTemperature(
    float temperature
);

void OLED_ShowHumidity(
    float humidity
);

void OLED_ShowLight(
    int lightLevel
);

void OLED_ShowRoomMonitor(
    float temperature,
    float humidity,
    int lightLevel
);

void OLED_SetPower(
    bool on
);

#endif