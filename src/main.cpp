#include "stm32f1xx.h"

static void UART_SendChar(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) { }
    USART1->DR = c;
}

static void UART_SendString(const char *s)
{
    while (*s)
    {
        UART_SendChar(*s++);
    }
}

int main(void)
{
    /* Enable clocks: GPIOA, AFIO, USART1 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;

    /* PA9 = TX: alternate function push-pull, 50MHz (CNF=10, MODE=11) */
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |=  (0xB << 4);

    /* PA10 = RX: floating input (CNF=01, MODE=00) */
    GPIOA->CRH &= ~(0xF << 8);
    GPIOA->CRH |=  (0x4 << 8);

    /* USART1 at 115200 baud, assuming 8MHz default HSI clock:
     * BRR = Fck / baud = 8000000 / 115200 ≈ 69.44 -> 0x45 (mantissa=4, fraction=5) */
    USART1->BRR = 0x45;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    for (;;)
    {
        UART_SendString("BCA182 FreeRTOS Multisensor\r\n");
        UART_SendString("System starting...\r\n");

        for (volatile int i = 0; i < 1000000; i++) { }
    }
}