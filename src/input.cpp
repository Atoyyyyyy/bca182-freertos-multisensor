#include "input.h"

#include "stm32f1xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app.h"
#include "hardware.h"
#include "serial_mutex.h"

// ============================================================
// ROTARY ENCODER
//
// PA4 = CLK
// PA5 = DT
//
// Polling is used with the current Wokwi setup.
//
// CLK falling edge:
//     DT HIGH -> +1
//     DT LOW  -> -1
// ============================================================

#define ENC_PORT        GPIOA
#define ENC_CLK_PIN    GPIO_PIN_4
#define ENC_DT_PIN     GPIO_PIN_5

static volatile int32_t encoderSteps = 0;

static GPIO_PinState lastClkState =
    GPIO_PIN_SET;

// ============================================================
// INITIALIZE ENCODER
// ============================================================

void Encoder_Init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};

    // --------------------------------------------------------
    // PA4 = CLK
    // --------------------------------------------------------

    gpio.Pin =
        ENC_CLK_PIN;

    gpio.Mode =
        GPIO_MODE_INPUT;

    gpio.Pull =
        GPIO_PULLUP;

    HAL_GPIO_Init(
        ENC_PORT,
        &gpio
    );

    // --------------------------------------------------------
    // PA5 = DT
    // --------------------------------------------------------

    gpio.Pin =
        ENC_DT_PIN;

    gpio.Mode =
        GPIO_MODE_INPUT;

    gpio.Pull =
        GPIO_PULLUP;

    HAL_GPIO_Init(
        ENC_PORT,
        &gpio
    );

    // --------------------------------------------------------
    // Save initial CLK state
    // --------------------------------------------------------

    lastClkState =
        HAL_GPIO_ReadPin(
            ENC_PORT,
            ENC_CLK_PIN
        );

    encoderSteps =
        0;

    // UART is shared by the application.
    // Route this message through the mutex.
    Serial_WriteString(
        "Encoder initialized: PA4 CLK, PA5 DT\r\n"
    );
}

// ============================================================
// UPDATE ENCODER
//
// Detects a falling edge on CLK and reads DT for direction.
// ============================================================

static void Encoder_Update()
{
    GPIO_PinState clkState =
        HAL_GPIO_ReadPin(
            ENC_PORT,
            ENC_CLK_PIN
        );

    // --------------------------------------------------------
    // Detect falling edge on CLK
    // --------------------------------------------------------

    if (lastClkState == GPIO_PIN_SET &&
        clkState == GPIO_PIN_RESET)
    {
        GPIO_PinState dtState =
            HAL_GPIO_ReadPin(
                ENC_PORT,
                ENC_DT_PIN
            );

        if (dtState == GPIO_PIN_SET)
        {
            encoderSteps++;
        }
        else
        {
            encoderSteps--;
        }
    }

    lastClkState =
        clkState;
}

// ============================================================
// READ + CLEAR ENCODER STEPS
// ============================================================

int Encoder_ReadStep()
{
    Encoder_Update();

    int32_t steps;

    taskENTER_CRITICAL();

    steps =
        encoderSteps;

    encoderSteps =
        0;

    taskEXIT_CRITICAL();

    return static_cast<int>(
        steps
    );
}

// ============================================================
// BUTTON UNUSED
// ============================================================

bool Encoder_ButtonPressed()
{
    return false;
}

// ============================================================
// INPUT TASK
//
// CLOCKWISE:
//     TEMPERATURE
//       -> HUMIDITY
//       -> MOTION
//       -> LIGHT
//       -> ALERT
//       -> TEMPERATURE
//
// COUNTERCLOCKWISE:
//     TEMPERATURE
//       -> ALERT
//       -> LIGHT
//       -> MOTION
//       -> HUMIDITY
//       -> TEMPERATURE
// ============================================================

void InputTask(void *argument)
{
    (void)argument;

    currentDisplayMode =
        DisplayMode::TEMPERATURE;

    Serial_WriteString(
        "InputTask started\r\n"
    );

    for (;;)
    {
        int steps =
            Encoder_ReadStep();

        // ----------------------------------------------------
        // CLOCKWISE
        // ----------------------------------------------------

        while (steps > 0)
        {
            switch (currentDisplayMode)
            {
                case DisplayMode::TEMPERATURE:

                    currentDisplayMode =
                        DisplayMode::HUMIDITY;

                    break;

                case DisplayMode::HUMIDITY:

                    currentDisplayMode =
                        DisplayMode::MOTION;

                    break;

                case DisplayMode::MOTION:

                    currentDisplayMode =
                        DisplayMode::LIGHT;

                    break;

                case DisplayMode::LIGHT:

                    currentDisplayMode =
                        DisplayMode::ALERT;

                    break;

                case DisplayMode::ALERT:

                    currentDisplayMode =
                        DisplayMode::TEMPERATURE;

                    break;
            }

            Serial_WriteString(
                "ENCODER: CLOCKWISE -> DISPLAY CHANGED\r\n"
            );

            steps--;
        }

        // ----------------------------------------------------
        // COUNTERCLOCKWISE
        // ----------------------------------------------------

        while (steps < 0)
        {
            switch (currentDisplayMode)
            {
                case DisplayMode::TEMPERATURE:

                    currentDisplayMode =
                        DisplayMode::ALERT;

                    break;

                case DisplayMode::ALERT:

                    currentDisplayMode =
                        DisplayMode::LIGHT;

                    break;

                case DisplayMode::LIGHT:

                    currentDisplayMode =
                        DisplayMode::MOTION;

                    break;

                case DisplayMode::MOTION:

                    currentDisplayMode =
                        DisplayMode::HUMIDITY;

                    break;

                case DisplayMode::HUMIDITY:

                    currentDisplayMode =
                        DisplayMode::TEMPERATURE;

                    break;
            }

            Serial_WriteString(
                "ENCODER: COUNTERCLOCKWISE -> DISPLAY CHANGED\r\n"
            );

            steps++;
        }

        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}