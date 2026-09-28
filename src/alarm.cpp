#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "app.h"
#include "buzzer.h"
#include "rtos_objects.h"
#include "serial_mutex.h"
#include "temperature_alarm.h"

#define UART1_WriteString Serial_WriteString
#define UART1_WriteInt    Serial_WriteInt
#define UART1_WriteFloat  Serial_WriteFloat

// ============================================================
// ALARM TASK
//
// Temperature < 18 C:
//     LOW TEMPERATURE
//     BUZZER ON
//
// 18 C to 30 C:
//     NORMAL
//     BUZZER OFF
//
// Temperature > 30 C:
//     HIGH TEMPERATURE
//     BUZZER ON
//
// INACTIVE:
//     BUZZER OFF
// ============================================================

void AlarmTask(void *argument)
{
    (void)argument;

    UART1_WriteString(
        "AlarmTask started\r\n"
    );

    SensorData data{};

    AlarmState alarmState =
        AlarmState::NORMAL;

    // --------------------------------------------------------
    // Initialize buzzer
    // --------------------------------------------------------

    if (!Buzzer_Init())
    {
        UART1_WriteString(
            "AlarmTask: Buzzer initialization FAILED\r\n"
        );
    }
    else
    {
        UART1_WriteString(
            "AlarmTask: Buzzer initialized\r\n"
        );
    }

    // Start OFF.
    Buzzer_Set(false);

    for (;;)
    {
        // ----------------------------------------------------
        // Wait for new sensor reading
        // ----------------------------------------------------

        if (alarmQueue != nullptr &&
            xQueueReceive(
                alarmQueue,
                &data,
                pdMS_TO_TICKS(100)
            ) == pdPASS)
        {
            AlarmState newState =
                evaluateTemperature(
                    data.temperature
                );

            // Only print when alarm state changes.
            if (newState != alarmState)
            {
                alarmState =
                    newState;

                switch (alarmState)
                {
                    case AlarmState::LOW_TEMPERATURE:

                        UART1_WriteString(
                            "AlarmTask: LOW TEMPERATURE\r\n"
                        );

                        UART1_WriteString(
                            "Buzzer: ON\r\n"
                        );

                        break;

                    case AlarmState::HIGH_TEMPERATURE:

                        UART1_WriteString(
                            "AlarmTask: HIGH TEMPERATURE\r\n"
                        );

                        UART1_WriteString(
                            "Buzzer: ON\r\n"
                        );

                        break;

                    case AlarmState::NORMAL:

                        UART1_WriteString(
                            "AlarmTask: NORMAL\r\n"
                        );

                        UART1_WriteString(
                            "Buzzer: OFF\r\n"
                        );

                        break;
                }
            }
        }

        // ----------------------------------------------------
        // Maintain buzzer state
        // ----------------------------------------------------

        bool systemActive =
            currentSystemState ==
            SystemState::ACTIVE;

        bool alarmActive =
            alarmState !=
            AlarmState::NORMAL;

        Buzzer_Set(
            systemActive &&
            alarmActive
        );

        vTaskDelay(
            pdMS_TO_TICKS(10)
        );
    }
}