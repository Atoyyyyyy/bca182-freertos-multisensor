#include "navigation.h"

DisplayMode NextDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
        case DisplayMode::TEMPERATURE:
            return DisplayMode::HUMIDITY;

        case DisplayMode::HUMIDITY:
            return DisplayMode::MOTION;

        case DisplayMode::MOTION:
            return DisplayMode::LIGHT;

        case DisplayMode::LIGHT:
            return DisplayMode::ALERT;

        case DisplayMode::ALERT:
            return DisplayMode::TEMPERATURE;
    }

    return DisplayMode::TEMPERATURE;
}

DisplayMode PreviousDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
        case DisplayMode::TEMPERATURE:
            return DisplayMode::ALERT;

        case DisplayMode::ALERT:
            return DisplayMode::LIGHT;

        case DisplayMode::LIGHT:
            return DisplayMode::MOTION;

        case DisplayMode::MOTION:
            return DisplayMode::HUMIDITY;

        case DisplayMode::HUMIDITY:
            return DisplayMode::TEMPERATURE;
    }

    return DisplayMode::TEMPERATURE;
}