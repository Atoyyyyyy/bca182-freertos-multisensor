#include "stm32f1xx.h"

#include "app.h"
#include "hardware.h"
#include "input.h"
#include "oled.h"
#include "buzzer.h"

// ============================================================
// GLOBALS
// ============================================================

QueueHandle_t sensorQueue = nullptr;

QueueHandle_t alarmQueue = nullptr;

EventGroupHandle_t systemEvents = nullptr;

volatile SystemState currentSystemState =
    SystemState::ACTIVE;

volatile DisplayMode currentDisplayMode =
    DisplayMode::TEMPERATURE;

// ============================================================
// TEMPERATURE ALARM LOGIC
// ============================================================

AlarmState evaluateTemperature(float temperature)
{
    if (temperature < 18.0f)
    {
        return AlarmState::LOW_TEMPERATURE;
    }

    if (temperature > 30.0f)
    {
        return AlarmState::HIGH_TEMPERATURE;
    }

    return AlarmState::NORMAL;
}

// ============================================================
// SENSOR TASK
//
// Reads DHT22 and LDR every 2 seconds.
//
// Sends data to:
//     sensorQueue -> DisplayTask
//     alarmQueue  -> AlarmTask
// ============================================================

void SensorTask(void *argument)
{
    (void)argument;

    UART1_WriteString(
        "SensorTask started\r\n"
    );

    TickType_t lastWakeTime =
        xTaskGetTickCount();

    for (;;)
    {
        SensorData data{};

        UART1_WriteString(
            "\r\n====================\r\n"
        );

        UART1_WriteString(
            "SensorTask: reading sensors\r\n"
        );

        // ----------------------------------------------------
        // DHT22
        // ----------------------------------------------------

        float temperature = 0.0f;
        float humidity = 0.0f;

        bool dhtOK =
            DHT22_Read(
                &temperature,
                &humidity
            );

        if (!dhtOK)
        {
            data.temperature = 0.0f;
            data.humidity = 0.0f;

            UART1_WriteString(
                "DHT22: READ FAILED\r\n"
            );
        }
        else
        {
            data.temperature =
                temperature;

            data.humidity =
                humidity;

            UART1_WriteString(
                "DHT22: OK\r\n"
            );

            UART1_WriteString(
                "Temperature: "
            );

            UART1_WriteFloat(
                data.temperature
            );

            UART1_WriteString(
                " C\r\n"
            );

            UART1_WriteString(
                "Humidity: "
            );

            UART1_WriteFloat(
                data.humidity
            );

            UART1_WriteString(
                " %\r\n"
            );
        }

        // ----------------------------------------------------
        // LDR
        // ----------------------------------------------------

        data.lightLevel =
            LDR_ReadPercent();

        UART1_WriteString(
            "Light: "
        );

        UART1_WriteInt(
            data.lightLevel
        );

        UART1_WriteString(
            " %\r\n"
        );

        // ----------------------------------------------------
        // PIR
        // ----------------------------------------------------

        data.motionDetected =
            PIR_Read();

        // ----------------------------------------------------
        // DISPLAY QUEUE
        // ----------------------------------------------------

        if (sensorQueue != nullptr)
        {
            xQueueOverwrite(
                sensorQueue,
                &data
            );
        }

        // ----------------------------------------------------
        // ALARM QUEUE
        // ----------------------------------------------------

        if (alarmQueue != nullptr)
        {
            xQueueOverwrite(
                alarmQueue,
                &data
            );
        }

        UART1_WriteString(
            "SensorTask: data queued\r\n"
        );

        UART1_WriteString(
            "====================\r\n"
        );

        // Required periodic timing.
        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
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

    // Start at TEMPERATURE.
    currentDisplayMode =
        DisplayMode::TEMPERATURE;

    UART1_WriteString(
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

            UART1_WriteString(
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

            UART1_WriteString(
                "ENCODER: COUNTERCLOCKWISE -> DISPLAY CHANGED\r\n"
            );

            steps++;
        }

        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}

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

                    // No OLED_ShowMotion() function exists
                    // in the current OLED module.
                    // Clear the screen for this page.

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

                    // The current OLED module has no
                    // OLED_ShowAlert() function.
                    //
                    // Keep this page blank for now.
                    // The AlarmTask controls the buzzer.

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