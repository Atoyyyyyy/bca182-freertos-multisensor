#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "hardware.h"

// ============================================================
// TIMING
// ============================================================

void DWT_Init()
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;

    uint32_t cycles =
        us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < cycles)
    {
    }
}

void delay_ms_blocking(uint32_t ms)
{
    while (ms--)
    {
        delay_us(1000);
    }
}

// ============================================================
// UART1
// ============================================================

void UART1_Init()
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // PA9 = USART1 TX
    GPIOA->CRH &= ~(0xFU << 4);
    GPIOA->CRH |= (0xBU << 4);

    // PA10 = USART1 RX
    GPIOA->CRH &= ~(0xFU << 8);
    GPIOA->CRH |= (0x4U << 8);

    USART1->BRR =
        8000000UL / 115200UL;

    USART1->CR1 =
        USART_CR1_TE |
        USART_CR1_RE |
        USART_CR1_UE;
}

void UART1_WriteChar(char c)
{
    while (!(USART1->SR & USART_SR_TXE))
    {
    }

    USART1->DR =
        static_cast<uint8_t>(c);
}

void UART1_WriteString(const char *str)
{
    if (str == nullptr)
    {
        return;
    }

    while (*str)
    {
        UART1_WriteChar(*str++);
    }
}

void UART1_WriteInt(int value)
{
    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%d",
        value
    );

    UART1_WriteString(buffer);
}

void UART1_WriteFloat(float value)
{
    if (value < 0.0f)
    {
        UART1_WriteChar('-');
        value = -value;
    }

    int whole =
        static_cast<int>(value);

    int decimal =
        static_cast<int>(
            (value - whole) * 10.0f + 0.5f
        );

    if (decimal >= 10)
    {
        decimal = 0;
        whole++;
    }

    UART1_WriteInt(whole);

    UART1_WriteChar('.');

    UART1_WriteChar(
        static_cast<char>(
            '0' + decimal
        )
    );
}

// ============================================================
// DHT22
// PA1 = DHT22 DATA
// ============================================================

void DHT22_Pin_Input()
{
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN;

    // PA1 input with pull-up
    GPIOA->CRL &= ~(0xFU << 4);
    GPIOA->CRL |= (0x8U << 4);

    GPIOA->BSRR =
        (1U << 1);
}

static void DHT22_Pin_Output()
{
    GPIOA->CRL &= ~(0xFU << 4);
    GPIOA->CRL |= (0x3U << 4);
}

static void DHT22_Pin_High()
{
    GPIOA->BSRR =
        (1U << 1);
}

static void DHT22_Pin_Low()
{
    GPIOA->BRR =
        (1U << 1);
}

static bool DHT22_Pin_Read()
{
    return
        (GPIOA->IDR & (1U << 1)) != 0;
}

bool DHT22_Read(
    float *temperature,
    float *humidity)
{
    if (temperature == nullptr ||
        humidity == nullptr)
    {
        return false;
    }

    DHT22_Pin_Output();

    DHT22_Pin_Low();

    delay_ms_blocking(2);

    DHT22_Pin_High();

    delay_us(30);

    DHT22_Pin_Input();

    uint32_t timeout = 10000;

    while (DHT22_Pin_Read())
    {
        if (--timeout == 0)
        {
            return false;
        }

        delay_us(1);
    }

    timeout = 10000;

    while (!DHT22_Pin_Read())
    {
        if (--timeout == 0)
        {
            return false;
        }

        delay_us(1);
    }

    timeout = 10000;

    while (DHT22_Pin_Read())
    {
        if (--timeout == 0)
        {
            return false;
        }

        delay_us(1);
    }

    uint8_t data[5] = {};

    const uint32_t cpuCyclesPerUs =
        SystemCoreClock / 1000000U;

    for (int i = 0; i < 40; i++)
    {
        timeout = 10000;

        while (!DHT22_Pin_Read())
        {
            if (--timeout == 0)
            {
                return false;
            }

            delay_us(1);
        }

        uint32_t pulseStart =
            DWT->CYCCNT;

        timeout = 10000;

        while (DHT22_Pin_Read())
        {
            if (--timeout == 0)
            {
                return false;
            }
        }

        uint32_t pulseCycles =
            DWT->CYCCNT - pulseStart;

        uint32_t pulseUs =
            pulseCycles / cpuCyclesPerUs;

        data[i / 8] <<= 1;

        if (pulseUs > 40)
        {
            data[i / 8] |= 1;
        }
    }

    UART1_WriteString(
        "DHT22 RAW: "
    );

    for (int i = 0; i < 5; i++)
    {
        UART1_WriteInt(
            static_cast<int>(data[i])
        );

        UART1_WriteChar(' ');
    }

    UART1_WriteString(
        "\r\n"
    );

    uint8_t checksum =
        static_cast<uint8_t>(
            data[0] +
            data[1] +
            data[2] +
            data[3]
        );

    if (checksum != data[4])
    {
        UART1_WriteString(
            "DHT22: CHECKSUM FAILED\r\n"
        );

        return false;
    }

    uint16_t rawHumidity =
        (static_cast<uint16_t>(data[0]) << 8) |
        data[1];

    *humidity =
        rawHumidity / 10.0f;

    uint16_t rawTemperature =
        (static_cast<uint16_t>(data[2]) << 8) |
        data[3];

    if (rawTemperature & 0x8000)
    {
        rawTemperature &= 0x7FFF;

        *temperature =
            -(rawTemperature / 10.0f);
    }
    else
    {
        *temperature =
            rawTemperature / 10.0f;
    }

    return true;
}

// ============================================================
// ADC / LDR
// PA0 = LDR
// ============================================================

void ADC1_Init()
{
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_ADC1EN;

    GPIOA->CRL &= ~(0xFU << 0);

    ADC1->CR2 = 0;

    ADC1->SMPR2 |=
        ADC_SMPR2_SMP0;

    ADC1->SQR1 = 0;
    ADC1->SQR2 = 0;
    ADC1->SQR3 = 0;

    ADC1->CR2 |=
        ADC_CR2_ADON;

    delay_us(10);

    ADC1->CR2 |=
        ADC_CR2_RSTCAL;

    while (ADC1->CR2 &
           ADC_CR2_RSTCAL)
    {
    }

    ADC1->CR2 |=
        ADC_CR2_CAL;

    while (ADC1->CR2 &
           ADC_CR2_CAL)
    {
    }
}

int LDR_ReadPercent()
{
    ADC1->SQR3 = 0;

    ADC1->CR2 |=
        ADC_CR2_ADON;

    while (!(ADC1->SR & ADC_SR_EOC))
    {
    }

    uint16_t raw =
        static_cast<uint16_t>(
            ADC1->DR
        );

    int percent =
        static_cast<int>(
            (raw * 100UL) / 4095UL
        );

    if (percent < 0)
    {
        percent = 0;
    }

    if (percent > 100)
    {
        percent = 100;
    }

    return percent;
}

// ============================================================
// PIR
// PA3 = PIR OUT
// ============================================================

void PIR_Init()
{
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN;

    // PA3 = input with pull-down
    GPIOA->CRL &= ~(0xFU << 12);
    GPIOA->CRL |=  (0x8U << 12);

    // Select pull-down
    GPIOA->BRR =
        GPIO_PIN_3;

    UART1_WriteString(
        "PIR initialized: PA3\r\n"
    );
}

bool PIR_Read()
{
    return
        (GPIOA->IDR & GPIO_PIN_3) != 0;
}

// ============================================================
// I2C1
// PB6 = SCL
// PB7 = SDA
// ============================================================

#define I2C_TIMEOUT 100000UL

static void I2C1_ResetPeripheral()
{
    I2C1->CR1 =
        I2C_CR1_SWRST;

    delay_us(10);

    I2C1->CR1 = 0;

    I2C1->CR2 = 8;

    I2C1->CCR = 40;

    I2C1->TRISE = 9;

    I2C1->OAR1 = 0x4000;

    I2C1->CR1 =
        I2C_CR1_PE |
        I2C_CR1_ACK;
}

void I2C1_Init()
{
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPBEN |
        RCC_APB2ENR_AFIOEN;

    RCC->APB1ENR |=
        RCC_APB1ENR_I2C1EN;

    (void)RCC->APB2ENR;
    (void)RCC->APB1ENR;

    GPIOB->CRL &=
        ~((0xFU << 24) |
          (0xFU << 28));

    GPIOB->CRL |=
        (0xBU << 24) |
        (0xBU << 28);

    GPIOB->BSRR =
        (1U << 6) |
        (1U << 7);

    I2C1->CR1 =
        I2C_CR1_SWRST;

    delay_us(10);

    I2C1->CR1 = 0;

    I2C1->CR2 = 8;

    I2C1->CCR = 40;

    I2C1->TRISE = 9;

    I2C1->OAR1 = 0x4000;

    I2C1->CR1 =
        I2C_CR1_PE |
        I2C_CR1_ACK;
}

static bool I2C1_DevicePresent(
    uint8_t address)
{
    I2C1->CR1 |=
        I2C_CR1_PE;

    I2C1->CR1 &=
        ~I2C_CR1_STOP;

    I2C1->SR1 &=
        ~(I2C_SR1_AF |
          I2C_SR1_BERR |
          I2C_SR1_ARLO);

    uint32_t timeout =
        I2C_TIMEOUT;

    while (I2C1->SR2 &
           I2C_SR2_BUSY)
    {
        if (--timeout == 0U)
        {
            I2C1_ResetPeripheral();
            return false;
        }
    }

    I2C1->CR1 |=
        I2C_CR1_START;

    timeout = I2C_TIMEOUT;

    while (!(I2C1->SR1 &
             I2C_SR1_SB))
    {
        if (--timeout == 0U)
        {
            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    I2C1->DR =
        static_cast<uint8_t>(
            address << 1
        );

    timeout = I2C_TIMEOUT;

    while (!(I2C1->SR1 &
             (I2C_SR1_ADDR |
              I2C_SR1_AF)))
    {
        if (--timeout == 0U)
        {
            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    if (I2C1->SR1 &
        I2C_SR1_AF)
    {
        I2C1->SR1 &=
            ~I2C_SR1_AF;

        I2C1->CR1 |=
            I2C_CR1_STOP;

        return false;
    }

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    I2C1->CR1 |=
        I2C_CR1_STOP;

    return true;
}

void I2C1_Scan()
{
    UART1_WriteString(
        "I2C SCAN START\r\n"
    );

    for (uint8_t address = 1;
         address < 127;
         address++)
    {
        if (I2C1_DevicePresent(address))
        {
            UART1_WriteString(
                "I2C DEVICE FOUND: 0x"
            );

            char hex[5];

            snprintf(
                hex,
                sizeof(hex),
                "%02X",
                address
            );

            UART1_WriteString(hex);

            UART1_WriteString(
                "\r\n"
            );
        }
    }

    UART1_WriteString(
        "I2C SCAN END\r\n"
    );
}

bool I2C1_WriteBytes(
    uint8_t address,
    const uint8_t *data,
    uint16_t length)
{
    if (data == nullptr ||
        length == 0)
    {
        return false;
    }

    I2C1->CR1 |=
        I2C_CR1_PE;

    I2C1->SR1 &=
        ~(I2C_SR1_AF |
          I2C_SR1_BERR |
          I2C_SR1_ARLO);

    I2C1->CR1 &=
        ~I2C_CR1_STOP;

    uint32_t timeout =
        I2C_TIMEOUT;

    while (I2C1->SR2 &
           I2C_SR2_BUSY)
    {
        if (--timeout == 0U)
        {
            UART1_WriteString(
                "I2C WRITE: BUSY TIMEOUT\r\n"
            );

            I2C1_ResetPeripheral();

            return false;
        }
    }

    I2C1->CR1 |=
        I2C_CR1_START;

    timeout = I2C_TIMEOUT;

    while (!(I2C1->SR1 &
             I2C_SR1_SB))
    {
        if (--timeout == 0U)
        {
            UART1_WriteString(
                "I2C WRITE: START TIMEOUT\r\n"
            );

            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    I2C1->DR =
        static_cast<uint8_t>(
            address << 1
        );

    timeout = I2C_TIMEOUT;

    while (!(I2C1->SR1 &
             (I2C_SR1_ADDR |
              I2C_SR1_AF)))
    {
        if (--timeout == 0U)
        {
            UART1_WriteString(
                "I2C WRITE: ADDRESS TIMEOUT\r\n"
            );

            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    if (I2C1->SR1 &
        I2C_SR1_AF)
    {
        UART1_WriteString(
            "I2C WRITE: ADDRESS NACK\r\n"
        );

        I2C1->SR1 &=
            ~I2C_SR1_AF;

        I2C1->CR1 |=
            I2C_CR1_STOP;

        return false;
    }

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    for (uint16_t i = 0;
         i < length;
         i++)
    {
        timeout = I2C_TIMEOUT;

        while (!(I2C1->SR1 &
                 I2C_SR1_TXE))
        {
            if (I2C1->SR1 &
                I2C_SR1_AF)
            {
                UART1_WriteString(
                    "I2C WRITE: DATA NACK\r\n"
                );

                UART1_WriteString(
                    "Byte: "
                );

                UART1_WriteInt(
                    static_cast<int>(i)
                );

                UART1_WriteString(
                    "\r\n"
                );

                I2C1->SR1 &=
                    ~I2C_SR1_AF;

                I2C1->CR1 |=
                    I2C_CR1_STOP;

                return false;
            }

            if (--timeout == 0U)
            {
                UART1_WriteString(
                    "I2C WRITE: TXE TIMEOUT\r\n"
                );

                I2C1->CR1 |=
                    I2C_CR1_STOP;

                return false;
            }
        }

        I2C1->DR =
            data[i];
    }

    timeout = I2C_TIMEOUT;

    while (!(I2C1->SR1 &
             I2C_SR1_BTF))
    {
        if (I2C1->SR1 &
            I2C_SR1_AF)
        {
            UART1_WriteString(
                "I2C WRITE: FINAL DATA NACK\r\n"
            );

            I2C1->SR1 &=
                ~I2C_SR1_AF;

            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }

        if (--timeout == 0U)
        {
            UART1_WriteString(
                "I2C WRITE: BTF TIMEOUT\r\n"
            );

            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    I2C1->CR1 |=
        I2C_CR1_STOP;

    timeout = I2C_TIMEOUT;

    while (I2C1->SR2 &
           I2C_SR2_BUSY)
    {
        if (--timeout == 0U)
        {
            UART1_WriteString(
                "I2C WRITE: STOP TIMEOUT\r\n"
            );

            I2C1_ResetPeripheral();

            return false;
        }
    }

    I2C1->CR1 &=
        ~I2C_CR1_STOP;

    return true;
}