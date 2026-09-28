#include "stm32f1xx.h"

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "input.h"
#include "app.h"
#include "hardware.h"
#include "oled.h"
#include "serial_mutex.h"
#include "sensors.h"
#include "display.h"
#include "alarm.h"
#include "motion.h"
#include "state.h"

// ============================================================
// SYSTEM CLOCK
// STM32F103 running from 8 MHz HSI
// ============================================================

static void SystemClock_Config()
{
    RCC_OscInitTypeDef RCC_OscInitStruct{};
    RCC_ClkInitTypeDef RCC_ClkInitStruct{};

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;

    HAL_RCC_OscConfig(
        &RCC_OscInitStruct
    );

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(
        &RCC_ClkInitStruct,
        FLASH_LATENCY_0
    );
}

// ============================================================
// MAIN
//
// The real firmware main() is excluded during PlatformIO
// unit testing so the test runner can provide its own main().
// ============================================================

#ifndef PIO_UNIT_TESTING

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    // ========================================================
    // HARDWARE CLOCKS
    // ========================================================

    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_IOPBEN;

    // ========================================================
    // BASIC INITIALIZATION
    // ========================================================

    DWT_Init();

    UART1_Init();

    // ========================================================
    // UART MUTEX
    //
    // UART1 is shared by multiple FreeRTOS tasks.
    // Create the mutex before any tasks start.
    // ========================================================

    SerialMutex_Init();

    UART1_WriteString(
        "\r\nBCA182 FreeRTOS Multisensor\r\n"
    );

    UART1_WriteString(
        "System starting...\r\n"
    );

    // ========================================================
    // SENSOR / GPIO INITIALIZATION
    // ========================================================

    // PA1 = DHT22
    DHT22_Pin_Input();

    // PA0 = LDR
    ADC1_Init();

    // PA3 = PIR
    PIR_Init();

    UART1_WriteString(
        "GPIO initialization complete.\r\n"
    );

    // ========================================================
    // ROTARY ENCODER INITIALIZATION
    //
    // PA4 = CLK
    // PA5 = DT
    // ========================================================

    Encoder_Init();

    // ========================================================
    // I2C / OLED INITIALIZATION
    //
    // PB6 = I2C1 SCL
    // PB7 = I2C1 SDA
    // ========================================================

    I2C1_Init();

    UART1_WriteString(
        "Initializing OLED...\r\n"
    );

    bool oledReady =
        OLED_Init();

    if (oledReady)
    {
        UART1_WriteString(
            "OLED initialization complete.\r\n"
        );

        // ----------------------------------------------------
        // Start OLED ON.
        // ----------------------------------------------------

        OLED_SetPower(true);

        UART1_WriteString(
            "OLED initially ON.\r\n"
        );

        // ----------------------------------------------------
        // Initial display = TEMPERATURE
        // ----------------------------------------------------

        OLED_ClearBuffer();

        OLED_ShowTemperature(
            25.4f
        );

        OLED_Update();

        UART1_WriteString(
            "Initial OLED screen sent.\r\n"
        );
    }
    else
    {
        UART1_WriteString(
            "OLED initialization FAILED.\r\n"
        );

        UART1_WriteString(
            "OLED screen skipped.\r\n"
        );
    }

    // ========================================================
    // SENSOR QUEUE
    // ========================================================

    sensorQueue =
        xQueueCreate(
            1,
            sizeof(SensorData)
        );

    if (sensorQueue == nullptr)
    {
        UART1_WriteString(
            "ERROR: Queue creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    UART1_WriteString(
        "Sensor queue created.\r\n"
    );

    // ========================================================
    // ALARM QUEUE
    // ========================================================

    alarmQueue =
        xQueueCreate(
            1,
            sizeof(SensorData)
        );

    if (alarmQueue == nullptr)
    {
        UART1_WriteString(
            "ERROR: Alarm queue creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    UART1_WriteString(
        "Alarm queue created.\r\n"
    );

    // ========================================================
    // SYSTEM EVENT GROUP
    // ========================================================

    systemEvents =
        xEventGroupCreate();

    if (systemEvents == nullptr)
    {
        UART1_WriteString(
            "ERROR: Event group creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    // Start system ACTIVE.
    xEventGroupClearBits(
        systemEvents,
        EVENT_MOTION
    );

    xEventGroupSetBits(
        systemEvents,
        EVENT_ACTIVE
    );

    currentSystemState =
        SystemState::ACTIVE;

    UART1_WriteString(
        "System event group created.\r\n"
    );

    UART1_WriteString(
        "System State: ACTIVE\r\n"
    );

    // ========================================================
    // MotionTask
    // Priority: 3
    // ========================================================

    if (xTaskCreate(
            MotionTask,
            "MotionTask",
            256,
            nullptr,
            3,
            nullptr
        ) != pdPASS)
    {
        UART1_WriteString(
            "ERROR: MotionTask creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    // ========================================================
    // StateTask
    // Priority: 3
    // ========================================================

    if (xTaskCreate(
            StateTask,
            "StateTask",
            256,
            nullptr,
            3,
            nullptr
        ) != pdPASS)
    {
        UART1_WriteString(
            "ERROR: StateTask creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    // ========================================================
    // InputTask
    // Priority: 3
    // ========================================================

    if (xTaskCreate(
            InputTask,
            "InputTask",
            256,
            nullptr,
            3,
            nullptr
        ) != pdPASS)
    {
        UART1_WriteString(
            "ERROR: InputTask creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    // ========================================================
    // SensorTask
    // Priority: 2
    // ========================================================

    if (xTaskCreate(
            SensorTask,
            "SensorTask",
            256,
            nullptr,
            2,
            nullptr
        ) != pdPASS)
    {
        UART1_WriteString(
            "ERROR: SensorTask creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    // ========================================================
    // AlarmTask
    // Priority: 2
    // ========================================================

    if (xTaskCreate(
            AlarmTask,
            "AlarmTask",
            256,
            nullptr,
            2,
            nullptr
        ) != pdPASS)
    {
        UART1_WriteString(
            "ERROR: AlarmTask creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    // ========================================================
    // DisplayTask
    // Priority: 1
    // ========================================================

    if (xTaskCreate(
            DisplayTask,
            "DisplayTask",
            512,
            nullptr,
            1,
            nullptr
        ) != pdPASS)
    {
        UART1_WriteString(
            "ERROR: DisplayTask creation failed!\r\n"
        );

        while (1)
        {
        }
    }

    // ========================================================
    // START FREERTOS
    // ========================================================

    UART1_WriteString(
        "FreeRTOS objects and tasks created.\r\n"
    );

    UART1_WriteString(
        "Starting FreeRTOS scheduler...\r\n"
    );

    vTaskStartScheduler();

    // Scheduler should never return.
    while (1)
    {
    }
}

#endif