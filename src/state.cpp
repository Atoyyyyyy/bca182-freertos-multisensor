#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "app.h"
#include "rtos_objects.h"
#include "serial_mutex.h"

#define UART1_WriteString Serial_WriteString

// ============================================================
// STATE TASK
//
// START:
//     ACTIVE
//
// Motion:
//     Reset 15-second inactivity timer
//
// No motion for 15 seconds:
//     INACTIVE
//
// Motion again while inactive:
//     ACTIVE
// ============================================================

void StateTask(void *argument)
{
    (void)argument;

    UART1_WriteString(
        "StateTask started\r\n"
    );

    SystemState state =
        SystemState::ACTIVE;

    TickType_t lastMotionTick =
        xTaskGetTickCount();

    const TickType_t inactivityTimeout =
        pdMS_TO_TICKS(15000);

    // --------------------------------------------------------
    // START ACTIVE
    // --------------------------------------------------------

    currentSystemState =
        SystemState::ACTIVE;

    if (systemEvents != nullptr)
    {
        xEventGroupSetBits(
            systemEvents,
            EVENT_ACTIVE
        );

        xEventGroupClearBits(
            systemEvents,
            EVENT_MOTION
        );
    }

    UART1_WriteString(
        "System State: ACTIVE\r\n"
    );

    for (;;)
    {
        // Wake every 250 ms even without motion.
        EventBits_t bits =
            xEventGroupWaitBits(
                systemEvents,
                EVENT_MOTION,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(250)
            );

        TickType_t now =
            xTaskGetTickCount();

        // ----------------------------------------------------
        // MOTION DETECTED
        // ----------------------------------------------------

        if ((bits & EVENT_MOTION) != 0)
        {
            lastMotionTick =
                now;

            if (state ==
                SystemState::INACTIVE)
            {
                state =
                    SystemState::ACTIVE;

                currentSystemState =
                    SystemState::ACTIVE;

                xEventGroupSetBits(
                    systemEvents,
                    EVENT_ACTIVE
                );

                UART1_WriteString(
                    "Motion detected while inactive.\r\n"
                );

                UART1_WriteString(
                    "System State: ACTIVE\r\n"
                );
            }
        }

        // ----------------------------------------------------
        // 15 SECOND INACTIVITY TIMEOUT
        // ----------------------------------------------------

        if (state ==
                SystemState::ACTIVE &&
            (now - lastMotionTick) >=
                inactivityTimeout)
        {
            state =
                SystemState::INACTIVE;

            currentSystemState =
                SystemState::INACTIVE;

            xEventGroupClearBits(
                systemEvents,
                EVENT_ACTIVE
            );

            UART1_WriteString(
                "15 seconds of inactivity reached.\r\n"
            );

            UART1_WriteString(
                "System State: INACTIVE\r\n"
            );

            UART1_WriteString(
                "OLED: OFF (INACTIVE)\r\n"
            );

            UART1_WriteString(
                "Buzzer: OFF (INACTIVE)\r\n"
            );
        }
    }
}