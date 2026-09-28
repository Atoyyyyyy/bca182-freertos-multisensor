#include "serial_mutex.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "hardware.h"

// ============================================================
// UART MUTEX
//
// UART1 is shared by multiple FreeRTOS tasks.
// This mutex prevents different tasks from writing to the
// serial output at the same time.
// ============================================================

static SemaphoreHandle_t uartMutex =
    nullptr;

// ============================================================
// INITIALIZE MUTEX
// ============================================================

void SerialMutex_Init()
{
    uartMutex =
        xSemaphoreCreateMutex();

    if (uartMutex == nullptr)
    {
        // This happens before the scheduler starts,
        // so use the raw UART directly.
        UART1_WriteString(
            "ERROR: UART mutex creation failed\r\n"
        );

        return;
    }

    UART1_WriteString(
        "UART mutex created\r\n"
    );
}

// ============================================================
// WRITE STRING
// ============================================================

void Serial_WriteString(
    const char *text)
{
    if (uartMutex == nullptr)
    {
        UART1_WriteString(
            text
        );

        return;
    }

    if (xSemaphoreTake(
            uartMutex,
            portMAX_DELAY
        ) == pdPASS)
    {
        UART1_WriteString(
            text
        );

        xSemaphoreGive(
            uartMutex
        );
    }
}

// ============================================================
// WRITE INTEGER
// ============================================================

void Serial_WriteInt(
    int value)
{
    if (uartMutex == nullptr)
    {
        UART1_WriteInt(
            value
        );

        return;
    }

    if (xSemaphoreTake(
            uartMutex,
            portMAX_DELAY
        ) == pdPASS)
    {
        UART1_WriteInt(
            value
        );

        xSemaphoreGive(
            uartMutex
        );
    }
}

// ============================================================
// WRITE FLOAT
// ============================================================

void Serial_WriteFloat(
    float value)
{
    if (uartMutex == nullptr)
    {
        UART1_WriteFloat(
            value
        );

        return;
    }

    if (xSemaphoreTake(
            uartMutex,
            portMAX_DELAY
        ) == pdPASS)
    {
        UART1_WriteFloat(
            value
        );

        xSemaphoreGive(
            uartMutex
        );
    }
}