#include "stm32f1xx.h"
#include <string.h>
#include <stdio.h>

/* ---------- Microsecond delay via DWT cycle counter ---------- */
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (8000000 / 1000000); /* 8 MHz HSI default clock */
    while ((DWT->CYCCNT - start) < ticks) { }
}

/* ---------- UART (USART1, PA9/PA10) ---------- */
static void UART_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |=  (0xB << 4);
    GPIOA->CRH &= ~(0xF << 8);
    GPIOA->CRH |=  (0x4 << 8);
    USART1->BRR = 0x45;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void UART_SendChar(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) { }
    USART1->DR = c;
}

static void UART_SendString(const char *s)
{
    while (*s) { UART_SendChar(*s++); }
}

/* ---------- DHT22 on PB0 ---------- */
#define DHT_PIN 0

static void DHT_SetOutput(void)
{
    GPIOB->CRL &= ~(0xF << (DHT_PIN * 4));
    GPIOB->CRL |=  (0x3 << (DHT_PIN * 4)); /* output push-pull, 50MHz */
}

static void DHT_SetInput(void)
{
    GPIOB->CRL &= ~(0xF << (DHT_PIN * 4));
    GPIOB->CRL |=  (0x8 << (DHT_PIN * 4)); /* input floating */
}

static void DHT_Write(int high)
{
    if (high) GPIOB->BSRR = (1 << DHT_PIN);
    else      GPIOB->BSRR = (1 << (DHT_PIN + 16));
}

static int DHT_Read(void)
{
    return (GPIOB->IDR & (1 << DHT_PIN)) ? 1 : 0;
}

/* Returns 1 on success, 0 on failure (timeout/checksum) */
static int DHT22_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};

    /* Start signal: pull low >=1ms, then release */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    DHT_SetOutput();
    DHT_Write(0);
    delay_us(1200);
    DHT_Write(1);
    delay_us(30);
    DHT_SetInput();

    /* Wait for sensor response: low ~80us, then high ~80us */
    uint32_t timeout = 10000;
    while (DHT_Read() == 1) { if (--timeout == 0) return 0; }
    timeout = 10000;
    while (DHT_Read() == 0) { if (--timeout == 0) return 0; }
    timeout = 10000;
    while (DHT_Read() == 1) { if (--timeout == 0) return 0; }

    /* Read 40 bits */
    for (int i = 0; i < 40; i++)
    {
        timeout = 10000;
        while (DHT_Read() == 0) { if (--timeout == 0) return 0; }

        delay_us(40); /* sample partway through the high pulse */

        int bit = DHT_Read();

        timeout = 10000;
        while (DHT_Read() == 1) { if (--timeout == 0) return 0; }

        data[i / 8] <<= 1;
        if (bit) data[i / 8] |= 1;
    }

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) return 0;

    *humidity    = ((data[0] << 8) | data[1]) / 10.0f;
    *temperature = (((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    if (data[2] & 0x80) *temperature = -*temperature;

    return 1;
}

static void UART_SendFloat1dp(float v)
{
    char buf[16];
    int whole = (int) v;
    int frac = (int)((v - whole) * 10);
    if (frac < 0) frac = -frac;
    snprintf(buf, sizeof(buf), "%d.%d", whole, frac);
    UART_SendString(buf);
}

int main(void)
{
    DWT_Init();
    UART_Init();

    UART_SendString("BCA182 FreeRTOS Multisensor\r\n");
    UART_SendString("System starting...\r\n");

    for (;;)
    {
        float temperature, humidity;

        if (DHT22_Read(&temperature, &humidity))
        {
            UART_SendString("Temperature: ");
            UART_SendFloat1dp(temperature);
            UART_SendString(" C\r\n");

            UART_SendString("Humidity: ");
            UART_SendFloat1dp(humidity);
            UART_SendString(" %\r\n");
        }
        else
        {
            UART_SendString("DHT22 read failed\r\n");
        }

        delay_us(2000000); /* ~2 second delay between reads, per DHT22 spec */
    }
}