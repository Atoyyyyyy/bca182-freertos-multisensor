#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* =========================================================
   FreeRTOS configuration for STM32F103C8
   ========================================================= */

/* CPU clock */
#define configCPU_CLOCK_HZ                    8000000UL

/* FreeRTOS tick rate */
#define configTICK_RATE_HZ                    1000

/* Scheduler */
#define configUSE_PREEMPTION                  1
#define configUSE_IDLE_HOOK                  0
#define configUSE_TICK_HOOK                  0

/* Tasks */
#define configMAX_PRIORITIES                  5
#define configMINIMAL_STACK_SIZE              128
#define configTOTAL_HEAP_SIZE                (10 * 1024)

/* Task names */
#define configMAX_TASK_NAME_LEN               16

/* Time */
#define configUSE_16_BIT_TICKS                0
#define configUSE_TIME_SLICING                1

/* Memory allocation */
#define configSUPPORT_DYNAMIC_ALLOCATION      1
#define configSUPPORT_STATIC_ALLOCATION       0

/* =========================================================
   Cortex-M3 interrupt configuration
   ========================================================= */

#define configPRIO_BITS                       4

/*
 * STM32F103 has 4 implemented priority bits.
 *
 * Lowest interrupt priority = 15
 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15

/*
 * FreeRTOS kernel interrupt priority
 */
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/*
 * Interrupt priority above which FreeRTOS API functions
 * cannot be called from an ISR.
 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* =========================================================
   FreeRTOS API inclusion
   ========================================================= */

#define INCLUDE_vTaskDelay                   1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xTaskGetSchedulerState      1

/* =========================================================
   Interrupt handlers
   ========================================================= */

#define vPortSVCHandler                     SVC_Handler
#define xPortPendSVHandler                  PendSV_Handler
#define xPortSysTickHandler                 SysTick_Handler

/* =========================================================
   Assertions
   ========================================================= */

#define configASSERT(x)                     \
    if ((x) == 0)                           \
    {                                       \
        taskDISABLE_INTERRUPTS();           \
        for (;;)                            \
        {                                   \
        }                                   \
    }

#endif /* FREERTOS_CONFIG_H */