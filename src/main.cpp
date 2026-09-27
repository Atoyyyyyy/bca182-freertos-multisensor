#include "stm32f1xx.h"
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/* =========================================================
   DWT MICROSECOND DELAY
   System clock = 8 MHz
   ========================================================= */

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;

    /* 8 MHz = 8 clock cycles per microsecond */
    uint32_t ticks = us * 8U;

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}

/* =========================================================
   UART1
   PA9  = TX
   PA10 = RX
   115200 baud
   ========================================================= */

static void UART_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN |
                    RCC_APB2ENR_AFIOEN |
                    RCC_APB2ENR_USART1EN;

    /* PA9 = Alternate Function Push-Pull, 50 MHz */
    GPIOA->CRH &= ~(0xFU << 4);
    GPIOA->CRH |= (0xBU << 4);

    /* PA10 = Input Floating */
    GPIOA->CRH &= ~(0xFU << 8);
    GPIOA->CRH |= (0x4U << 8);

    /* 115200 baud at 8 MHz */
    USART1->BRR = 0x45;

    USART1->CR1 = USART_CR1_UE |
                  USART_CR1_TE |
                  USART_CR1_RE;
}

static void UART_SendChar(char c)
{
    while (!(USART1->SR & USART_SR_TXE))
    {
    }

    USART1->DR = (uint16_t)c;
}

static void UART_SendString(const char *s)
{
    while (*s)
    {
        UART_SendChar(*s);
        s++;
    }
}

/*
 * Send a floating-point value with one decimal place.
 *
 * Example:
 * 24.0
 * 55.3
 */
static void UART_SendFloat1dp(float value)
{
    int whole = (int)value;

    int fraction =
        (int)((value - (float)whole) * 10.0f);

    if (fraction < 0)
    {
        fraction = -fraction;
    }

    /* Handle negative values */
    if (value < 0.0f)
    {
        UART_SendChar('-');

        whole = -whole;
    }

    UART_SendChar(
        (char)('0' + (whole / 10) % 10)
    );

    UART_SendChar(
        (char)('0' + whole % 10)
    );

    UART_SendChar('.');

    UART_SendChar(
        (char)('0' + fraction % 10)
    );
}

/* =========================================================
   DHT22
   PB0 = DATA
   ========================================================= */

#define DHT_PIN 0U

static void DHT_SetOutput(void)
{
    GPIOB->CRL &= ~(0xFU << (DHT_PIN * 4U));

    /*
     * General purpose output push-pull,
     * 50 MHz
     */
    GPIOB->CRL |= (0x3U << (DHT_PIN * 4U));
}

static void DHT_SetInput(void)
{
    GPIOB->CRL &= ~(0xFU << (DHT_PIN * 4U));

    /*
     * Input with pull-up/pull-down
     */
    GPIOB->CRL |= (0x8U << (DHT_PIN * 4U));
}

static void DHT_Write(int high)
{
    if (high)
    {
        GPIOB->BSRR = (1U << DHT_PIN);
    }
    else
    {
        GPIOB->BSRR = (1U << (DHT_PIN + 16U));
    }
}

static int DHT_Read(void)
{
    return (GPIOB->IDR & (1U << DHT_PIN)) ? 1 : 0;
}

/* =========================================================
   DHT22 READ
   Returns:
      1 = success
      0 = failure
   ========================================================= */

static int DHT22_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};

    /* Enable GPIOB */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    /* -----------------------------------------------------
       START SIGNAL
       ----------------------------------------------------- */

    DHT_SetOutput();

    /* Pull LOW for at least 1 ms */
    DHT_Write(0);

    delay_us(1200);

    /* Release line */
    DHT_Write(1);

    delay_us(30);

    /* Change to input */
    DHT_SetInput();

    /* -----------------------------------------------------
       DHT22 RESPONSE
       ----------------------------------------------------- */

    uint32_t timeout;

    timeout = 10000;

    while (DHT_Read() == 1)
    {
        if (--timeout == 0)
        {
            return 0;
        }
    }

    timeout = 10000;

    while (DHT_Read() == 0)
    {
        if (--timeout == 0)
        {
            return 0;
        }
    }

    timeout = 10000;

    while (DHT_Read() == 1)
    {
        if (--timeout == 0)
        {
            return 0;
        }
    }

    /* -----------------------------------------------------
       READ 40 BITS
       ----------------------------------------------------- */

    for (int i = 0; i < 40; i++)
    {
        timeout = 10000;

        /* Wait for bit to start */
        while (DHT_Read() == 0)
        {
            if (--timeout == 0)
            {
                return 0;
            }
        }

        /*
         * Sample after approximately 40 us.
         *
         * Short pulse = 0
         * Long pulse = 1
         */
        delay_us(40);

        int bit = DHT_Read();

        timeout = 10000;

        /* Wait for bit to finish */
        while (DHT_Read() == 1)
        {
            if (--timeout == 0)
            {
                return 0;
            }
        }

        data[i / 8] <<= 1;

        if (bit)
        {
            data[i / 8] |= 1U;
        }
    }

    /* -----------------------------------------------------
       CHECKSUM
       ----------------------------------------------------- */

    uint8_t checksum =
        (uint8_t)(
            data[0] +
            data[1] +
            data[2] +
            data[3]
        );

    if (checksum != data[4])
    {
        return 0;
    }

    /* -----------------------------------------------------
       HUMIDITY
       ----------------------------------------------------- */

    uint16_t humidityRaw =
        (uint16_t)(
            ((uint16_t)data[0] << 8) |
            data[1]
        );

    *humidity =
        humidityRaw / 10.0f;

    /* -----------------------------------------------------
       TEMPERATURE
       ----------------------------------------------------- */

    uint16_t temperatureRaw =
        (uint16_t)(
            (((uint16_t)data[2] & 0x7FU) << 8) |
            data[3]
        );

    *temperature =
        temperatureRaw / 10.0f;

    /* Negative temperature */
    if (data[2] & 0x80U)
    {
        *temperature =
            -*temperature;
    }

    return 1;
}

/* =========================================================
   ADC1
   PA0 = LDR ANALOG INPUT
   ========================================================= */

static void ADC_Init(void)
{
    /* Enable ADC1 and GPIOA */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN |
                    RCC_APB2ENR_IOPAEN;

    /* PA0 = analog input */
    GPIOA->CRL &= ~(0xFU << 0);

    /* Enable ADC */
    ADC1->CR2 |= ADC_CR2_ADON;

    delay_us(10);

    /* Reset calibration */
    ADC1->CR2 |= ADC_CR2_RSTCAL;

    while (ADC1->CR2 & ADC_CR2_RSTCAL)
    {
    }

    /* Start calibration */
    ADC1->CR2 |= ADC_CR2_CAL;

    while (ADC1->CR2 & ADC_CR2_CAL)
    {
    }
}

static uint16_t ADC_Read(uint8_t channel)
{
    /* Select ADC channel */
    ADC1->SQR3 = channel;

    /* Start conversion */
    ADC1->CR2 |= ADC_CR2_ADON;

    /* Wait for conversion */
    while (!(ADC1->SR & ADC_SR_EOC))
    {
    }

    return (uint16_t)ADC1->DR;
}

/* =========================================================
   LDR
   ADC 0-4095 -> 0-100%
   ========================================================= */

static int LDR_ReadPercent(void)
{
    uint16_t raw = ADC_Read(0);

    return ((uint32_t)raw * 100U) / 4095U;
}

/* =========================================================
   SECTION 22
   SENSOR TASK
   ========================================================= */

static void SensorTask(void *argument)
{
    (void)argument;

    /*
     * Initial wake-up time.
     *
     * vTaskDelayUntil() uses this to maintain
     * a regular 2-second periodic schedule.
     */
    TickType_t lastWakeTime =
        xTaskGetTickCount();

    for (;;)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        /* -------------------------------------------------
           READ DHT22
           ------------------------------------------------- */

        if (DHT22_Read(
                &temperature,
                &humidity))
        {
            UART_SendString(
                "Temperature: "
            );

            UART_SendFloat1dp(
                temperature
            );

            UART_SendString(
                " C\r\n"
            );

            UART_SendString(
                "Humidity: "
            );

            UART_SendFloat1dp(
                humidity
            );

            UART_SendString(
                " %\r\n"
            );
        }
        else
        {
            UART_SendString(
                "DHT22 read failed\r\n"
            );
        }

        /* -------------------------------------------------
           READ LDR
           ------------------------------------------------- */

        int lightPercent =
            LDR_ReadPercent();

        UART_SendString(
            "Light: "
        );

        /*
         * Print integer without snprintf().
         */
        if (lightPercent >= 100)
        {
            UART_SendString("100");
        }
        else if (lightPercent >= 10)
        {
            UART_SendChar(
                (char)('0' +
                    (lightPercent / 10))
            );

            UART_SendChar(
                (char)('0' +
                    (lightPercent % 10))
            );
        }
        else
        {
            UART_SendChar(
                (char)('0' + lightPercent)
            );
        }

        UART_SendString(
            " %\r\n"
        );

        UART_SendString(
            "--------------------\r\n"
        );

        /* -------------------------------------------------
           PERIODIC DELAY
           -------------------------------------------------

           SensorTask runs every 2 seconds.

           vTaskDelayUntil() maintains the periodic
           schedule and reduces timing drift.
           ------------------------------------------------- */

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}

/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    /* -----------------------------------------------------
       INITIALIZE HARDWARE
       ----------------------------------------------------- */

    DWT_Init();

    UART_Init();

    ADC_Init();

    /* -----------------------------------------------------
       STARTUP MESSAGE
       ----------------------------------------------------- */

    UART_SendString(
        "BCA182 FreeRTOS Multisensor\r\n"
    );

    UART_SendString(
        "System starting...\r\n"
    );

    /* -----------------------------------------------------
       CREATE SENSOR TASK
       ----------------------------------------------------- */

    BaseType_t taskResult;

    taskResult = xTaskCreate(
        SensorTask,
        "SensorTask",
        256,
        NULL,
        2,
        NULL
    );

    /* -----------------------------------------------------
       CHECK TASK CREATION
       ----------------------------------------------------- */

    if (taskResult != pdPASS)
    {
        UART_SendString(
            "ERROR: SensorTask creation failed\r\n"
        );

        while (1)
        {
        }
    }

    /* -----------------------------------------------------
       START FREERTOS
       ----------------------------------------------------- */

    vTaskStartScheduler();

    /*
     * Normally never reached.
     */
    while (1)
    {
    }
}