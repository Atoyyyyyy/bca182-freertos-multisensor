#include "stm32f1xx.h"
#include <string.h>

static void LED_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x2 << 20);
}

static void LED_On(void)  { GPIOC->ODR &= ~GPIO_ODR_ODR13; }
static void LED_Off(void) { GPIOC->ODR |=  GPIO_ODR_ODR13; }

static void Delay_Crude(volatile uint32_t count)
{
    while (count--) { }
}

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

int main(void)
{
    LED_Init();
    UART_Init();

    for (int i = 0; i < 3; i++)
    {
        LED_On();
        Delay_Crude(800000);
        LED_Off();
        Delay_Crude(800000);
    }

    for (;;)
    {
        UART_SendString("BCA182 FreeRTOS Multisensor\r\n");
        UART_SendString("System starting...\r\n");
        Delay_Crude(1000000);
    }
}