#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "app.h"
#include "hardware.h"
#include "rtos_objects.h"
#include "serial_mutex.h"

#define UART1_WriteString Serial_WriteString

// ============================================================
// MOTION TASK
//
// PIR is checked every 100 ms.
//
// While PIR is HIGH:
//     EVENT_MOTION is set
// ============================================================

void MotionTask(void *argument)
{
    (void)argument;

    UART1_WriteString(
        "MotionTask started\r\n"
    );

    bool lastLevel = false;

    for (;;)
    {
        bool level =
            PIR_Read();

        if (level)
        {
            if (systemEvents != nullptr)
            {
                xEventGroupSetBits(
                    systemEvents,
                    EVENT_MOTION
                );
            }

            if (!lastLevel)
            {
                UART1_WriteString(
                    "PIR: HIGH\r\n"
                );

                UART1_WriteString(
                    "Motion detected\r\n"
                );
            }
        }
        else
        {
            if (lastLevel)
            {
                UART1_WriteString(
                    "PIR: LOW\r\n"
                );

                UART1_WriteString(
                    "Motion ended\r\n"
                );
            }
        }

        lastLevel =
            level;

        vTaskDelay(
            pdMS_TO_TICKS(100)
        );
    }
}