#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"

static void LED_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x2 << 20);
}

static void LED_On(void)  { GPIOC->ODR &= ~GPIO_ODR_ODR13; }
static void LED_Off(void) { GPIOC->ODR |=  GPIO_ODR_ODR13; }
static void LED_Toggle(void) { GPIOC->ODR ^= GPIO_ODR_ODR13; }

static void Delay_Crude(volatile uint32_t count)
{
    while (count--) { }
}

extern "C" void HardFault_Handler(void)
{
    for (;;)
    {
        LED_Toggle();
        Delay_Crude(200000);
    }
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

static void UART_SendNumber(int n)
{
    char buf[12];
    int i = 0;
    if (n == 0) { UART_SendChar('0'); return; }
    if (n < 0) { UART_SendChar('-'); n = -n; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) { UART_SendChar(buf[--i]); }
}

static void TaskA(void *pvParameters)
{
    (void) pvParameters;
    for (;;)
    {
        LED_Toggle();
        UART_SendString("Task A running\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void TaskB(void *pvParameters)
{
    (void) pvParameters;
    for (;;)
    {
        UART_SendString("Task B running\r\n");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void) xTask;
    UART_SendString("!!! STACK OVERFLOW: ");
    UART_SendString(pcTaskName);
    UART_SendString(" !!!\r\n");
    for (;;) { }
}

int main(void)
{
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3);

    LED_Init();
    UART_Init();

    for (int i = 0; i < 3; i++)
    {
        LED_On();
        Delay_Crude(800000);
        LED_Off();
        Delay_Crude(800000);
    }

    UART_SendString("Reached main(), creating tasks...\r\n");

    BaseType_t resultA = xTaskCreate(TaskA, "TaskA", 128, NULL, 2, NULL);
    BaseType_t resultB = xTaskCreate(TaskB, "TaskB", 128, NULL, 2, NULL);

    UART_SendString("TaskA create result: ");
    UART_SendNumber(resultA);
    UART_SendString("\r\n");

    UART_SendString("TaskB create result: ");
    UART_SendNumber(resultB);
    UART_SendString("\r\n");

    UART_SendString("Free heap: ");
    UART_SendNumber((int) xPortGetFreeHeapSize());
    UART_SendString(" bytes\r\n");

    UART_SendString("Starting scheduler...\r\n");

    vTaskStartScheduler();

    UART_SendString("!!! SCHEDULER FAILED TO START !!!\r\n");
    for (;;)
    {
        LED_On();
        Delay_Crude(100000);
        LED_Off();
        Delay_Crude(100000);
    }
}