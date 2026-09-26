#include "stm32f1xx_hal.h"

static void SystemClock_Config(void);

void app_main(void)
{
    for (;;)
    {
        HAL_Delay(1000);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    app_main();

    return 0;
}

static void SystemClock_Config(void)
{
}