#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

// ============================================================
// SCHEDULER
// ============================================================

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1

// STM32F103 is running from the 8 MHz HSI.
#define configCPU_CLOCK_HZ                      8000000UL

// Wokwi compatibility port uses TIM3 at 20 Hz.
// 1 FreeRTOS tick = 50 ms.
#define configTICK_RATE_HZ                      20

#define configUSE_16_BIT_TICKS                  0

#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)
#define configMAX_TASK_NAME_LEN                 16
#define configIDLE_SHOULD_YIELD                 1

// ============================================================
// MEMORY
// ============================================================

#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

#define configTOTAL_HEAP_SIZE                   ((size_t)(10 * 1024))

// ============================================================
// FEATURES
// ============================================================

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_EVENT_GROUPS                  1

// ============================================================
// HOOKS
// ============================================================

#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0

#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            0

// ============================================================
// API FUNCTIONS
// ============================================================

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelete                     1

#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_xTaskGetSchedulerState          1

// ============================================================
// INTERRUPT PRIORITIES
// ============================================================

#define configPRIO_BITS                         4

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15

#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << \
    (8 - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << \
    (8 - configPRIO_BITS))

// ============================================================
// ASSERT
// ============================================================
//
// FreeRTOS kernel source is compiled as C.
// rtos_hooks.cpp defines vAssertCalled() with C linkage.
// The declaration must therefore use C linkage too.
// ============================================================

#ifndef __ASSEMBLER__

#ifdef __cplusplus
extern "C"
{
#endif

void vAssertCalled(
    const char *file,
    int line
);

#ifdef __cplusplus
}
#endif

#endif

#define configASSERT(x) \
    do \
    { \
        if ((x) == 0) \
        { \
            vAssertCalled( \
                __FILE__, \
                __LINE__ \
            ); \
        } \
    } while (0)

// ============================================================
// FREE RTOS HANDLER NAMES
//
// The Wokwi compatibility port uses TIM3 for the RTOS tick.
// SVC/PendSV/SysTick are kept only for compatibility.
// ============================================================

#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif