#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

enum class AlarmState
{
    NORMAL,
    LOW_TEMPERATURE,
    HIGH_TEMPERATURE
};

enum class SystemState
{
    ACTIVE,
    INACTIVE
};

enum class DisplayMode
{
    TEMPERATURE,
    HUMIDITY,
    MOTION,
    LIGHT,
    ALERT
};

#endif