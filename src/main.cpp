#include "stm32f1xx.h"
#include <string.h>

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
    UART_Init();

    for (;;)
    {
        UART_SendString("BCA182 FreeRTOS Multisensor\r\n");
        UART_SendString("System starting...\r\n");

        for (volatile int i = 0; i < 1000000; i++) { }
    }
}