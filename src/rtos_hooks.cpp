#include "FreeRTOS.h"
#include "task.h"

#include "stm32f1xx_hal.h"
#include "stm32f1xx.h"

#include "hardware.h"

// ============================================================
// ASSERT HANDLER
// ============================================================
//
// FreeRTOS is compiled as C, so this function must use C
// linkage when called from FreeRTOS C source files.
// ============================================================

extern "C" void vAssertCalled(
    const char *file,
    int line)
{
    __disable_irq();

    UART1_WriteString(
        "\r\nASSERT FAILED\r\n"
    );

    UART1_WriteString(
        "File: "
    );

    UART1_WriteString(
        file
    );

    UART1_WriteString(
        "\r\nLine: "
    );

    UART1_WriteInt(
        line
    );

    UART1_WriteString(
        "\r\n"
    );

    for (;;)
    {
    }
}

// ============================================================
// STACK OVERFLOW HOOK
// ============================================================

extern "C" void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *taskName)
{
    (void)task;

    __disable_irq();

    UART1_WriteString(
        "\r\nSTACK OVERFLOW\r\n"
    );

    UART1_WriteString(
        "Task: "
    );

    UART1_WriteString(
        taskName
    );

    UART1_WriteString(
        "\r\n"
    );

    for (;;)
    {
    }
}

// ============================================================
// xPortConsumeTickYield
//
// IMPORTANT:
// This function is implemented in port.c, which is C.
// Therefore the declaration must use C linkage here.
// ============================================================

extern "C" BaseType_t xPortConsumeTickYield(void);

// ============================================================
// IDLE HOOK
//
// The Wokwi compatibility port uses TIM3 as the FreeRTOS
// tick. The idle hook waits for the timer and then performs
// the pending context switch from Thread mode.
// ============================================================

extern "C" void vApplicationIdleHook(void)
{
    __WFI();

    if (xPortConsumeTickYield() != pdFALSE)
    {
        taskYIELD();
    }
}

// ============================================================
// HAL TIMER CALLBACK
// ============================================================
//
// TIM4 is used for HAL timing before the scheduler starts.
// ============================================================

extern "C" void HAL_TIM_PeriodElapsedCallback(
    TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        if (xTaskGetSchedulerState() ==
            taskSCHEDULER_NOT_STARTED)
        {
            HAL_IncTick();
        }
    }
}