#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "app.h"
#include "hardware.h"
#include "rtos_objects.h"
#include "serial_mutex.h"

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

    Serial_WriteString(
        "SensorTask started\r\n"
    );

    TickType_t lastWakeTime =
        xTaskGetTickCount();

    for (;;)
    {
        SensorData data{};

        Serial_WriteString(
            "\r\n====================\r\n"
        );

        Serial_WriteString(
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

            Serial_WriteString(
                "DHT22: READ FAILED\r\n"
            );
        }
        else
        {
            data.temperature =
                temperature;

            data.humidity =
                humidity;

            Serial_WriteString(
                "DHT22: OK\r\n"
            );

            Serial_WriteString(
                "Temperature: "
            );

            Serial_WriteFloat(
                data.temperature
            );

            Serial_WriteString(
                " C\r\n"
            );

            Serial_WriteString(
                "Humidity: "
            );

            Serial_WriteFloat(
                data.humidity
            );

            Serial_WriteString(
                " %\r\n"
            );
        }

        // ----------------------------------------------------
        // LDR
        // ----------------------------------------------------

        data.lightLevel =
            LDR_ReadPercent();

        Serial_WriteString(
            "Light: "
        );

        Serial_WriteInt(
            data.lightLevel
        );

        Serial_WriteString(
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

        Serial_WriteString(
            "SensorTask: data queued\r\n"
        );

        Serial_WriteString(
            "====================\r\n"
        );

        // Required periodic timing.
        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}