#include "temperature_alarm.h"

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
