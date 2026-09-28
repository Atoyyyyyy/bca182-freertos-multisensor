#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"

#include "app.h"
#include "oled.h"
#include "rtos_objects.h"
#include "serial_mutex.h"

#define UART1_WriteString Serial_WriteString
#define UART1_WriteInt    Serial_WriteInt
#define UART1_WriteFloat  Serial_WriteFloat

// ============================================================
// DISPLAY TASK
//
// ACTIVE:
//     OLED ON
//
// INACTIVE:
//     OLED OFF
//
// DISPLAY ORDER:
//     TEMPERATURE
//     HUMIDITY
//     MOTION
//     LIGHT
//     ALERT
// ============================================================

void DisplayTask(void *argument)
{
    (void)argument;

    UART1_WriteString(
        "DisplayTask started\r\n"
    );

    SensorData data{};

    bool haveSensorData =
        false;

    DisplayMode lastDisplayMode =
        currentDisplayMode;

    bool firstDisplay =
        true;

    bool oledOn =
        false;

    // --------------------------------------------------------
    // START OLED ON
    // --------------------------------------------------------

    OLED_SetPower(true);

    oledOn =
        true;

    UART1_WriteString(
        "OLED: ON (STARTUP)\r\n"
    );

    for (;;)
    {
        if (systemEvents == nullptr)
        {
            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

        EventBits_t stateBits =
            xEventGroupGetBits(
                systemEvents
            );

        // ====================================================
        // INACTIVE
        // ====================================================

        if ((stateBits & EVENT_ACTIVE) == 0)
        {
            if (oledOn)
            {
                OLED_SetPower(false);

                oledOn =
                    false;

                UART1_WriteString(
                    "OLED: OFF (INACTIVE)\r\n"
                );
            }

            vTaskDelay(
                pdMS_TO_TICKS(50)
            );

            continue;
        }

        // ====================================================
        // ACTIVE
        // ====================================================

        if (!oledOn)
        {
            OLED_SetPower(true);

            oledOn =
                true;

            firstDisplay =
                true;

            UART1_WriteString(
                "OLED: ON (ACTIVE)\r\n"
            );
        }

        // ----------------------------------------------------
        // Get newest sensor data
        // ----------------------------------------------------

        bool newSensorData =
            false;

        if (sensorQueue != nullptr &&
            xQueueReceive(
                sensorQueue,
                &data,
                pdMS_TO_TICKS(100)
            ) == pdPASS)
        {
            haveSensorData =
                true;

            newSensorData =
                true;
        }

        // ----------------------------------------------------
        // Read current display mode
        // ----------------------------------------------------

        DisplayMode mode =
            currentDisplayMode;

        // ----------------------------------------------------
        // Redraw when:
        //
        // 1. First display
        // 2. Encoder changed mode
        // 3. New sensor data arrived
        // ----------------------------------------------------

        if (haveSensorData &&
            (firstDisplay ||
             mode != lastDisplayMode ||
             newSensorData))
        {
            firstDisplay =
                false;

            lastDisplayMode =
                mode;

            OLED_ClearBuffer();

            switch (mode)
            {
                // --------------------------------------------
                // TEMPERATURE
                // --------------------------------------------

                case DisplayMode::TEMPERATURE:

                    UART1_WriteString(
                        "OLED: TEMPERATURE\r\n"
                    );

                    OLED_ShowTemperature(
                        data.temperature
                    );

                    break;

                // --------------------------------------------
                // HUMIDITY
                // --------------------------------------------

                case DisplayMode::HUMIDITY:

                    UART1_WriteString(
                        "OLED: HUMIDITY\r\n"
                    );

                    OLED_ShowHumidity(
                        data.humidity
                    );

                    break;

                // --------------------------------------------
                // MOTION
                // --------------------------------------------

                case DisplayMode::MOTION:

                    UART1_WriteString(
                        "OLED: MOTION\r\n"
                    );

                    OLED_ClearBuffer();

                    break;

                // --------------------------------------------
                // LIGHT
                // --------------------------------------------

                case DisplayMode::LIGHT:

                    UART1_WriteString(
                        "OLED: LIGHT\r\n"
                    );

                    OLED_ShowLight(
                        data.lightLevel
                    );

                    break;

                // --------------------------------------------
                // ALERT
                // --------------------------------------------

                case DisplayMode::ALERT:

                    UART1_WriteString(
                        "OLED: ALERT\r\n"
                    );

                    OLED_ClearBuffer();

                    break;
            }

            OLED_Update();
        }

        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}