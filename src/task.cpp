#include "app.h"

// ============================================================
// TEMPERATURE ALARM LOGIC
// ============================================================

AlarmState evaluateTemperature(float temperature)
{
    if (temperature < 18.0f)
    {
        return AlarmState::LOW_TEMPERATURE;
    }

    if (temperature > 30.0f)
    {
        return AlarmState::HIGH_TEMPERATURE;
    }

    return AlarmState::NORMAL;
}