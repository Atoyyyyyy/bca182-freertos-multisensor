#include "stm32f1xx.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// ============================================================
// SENSOR DATA
// ============================================================

struct SensorData {
    float temperature;
    float humidity;
    int lightLevel;
    bool motionDetected;
};

static QueueHandle_t sensorQueue;

// ============================================================
// ALARM LOGIC
// SECTION 30
// ============================================================

enum class AlarmState {
    NORMAL,
    LOW_TEMPERATURE,
    HIGH_TEMPERATURE
};

// Temperature thresholds
static constexpr float LOW_TEMPERATURE_THRESHOLD = 18.0f;
static constexpr float HIGH_TEMPERATURE_THRESHOLD = 30.0f;

// Pure temperature decision logic.
// This function does not access any hardware.
static AlarmState evaluateTemperature(
    float temperature)
{
    if (temperature < LOW_TEMPERATURE_THRESHOLD) {
        return AlarmState::LOW_TEMPERATURE;
    }

    if (temperature > HIGH_TEMPERATURE_THRESHOLD) {
        return AlarmState::HIGH_TEMPERATURE;
    }

    return AlarmState::NORMAL;
}

// ============================================================
// DISPLAY MODE
// ============================================================

enum class DisplayMode {
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
};

static volatile DisplayMode currentDisplayMode =
    DisplayMode::TEMPERATURE;

// ============================================================
// DWT DELAY
// STM32F103 default clock = 8 MHz HSI
// ============================================================

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * 8U;

    while ((DWT->CYCCNT - start) < ticks) {
    }
}

static void delay_ms_blocking(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}

// ============================================================
// UART1
// PA9  = TX
// PA10 = RX
// 115200 baud @ 8 MHz
// ============================================================

static void UART1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN |
                    RCC_APB2ENR_AFIOEN |
                    RCC_APB2ENR_USART1EN;

    // PA9 = Alternate Function Push-Pull, 50 MHz
    GPIOA->CRH &= ~(0xFU << 4);
    GPIOA->CRH |=  (0xBU << 4);

    // PA10 = Input Floating
    GPIOA->CRH &= ~(0xFU << 8);
    GPIOA->CRH |=  (0x4U << 8);

    // 115200 baud @ 8 MHz
    USART1->BRR = 0x45;

    USART1->CR1 =
        USART_CR1_UE |
        USART_CR1_TE |
        USART_CR1_RE;
}

static void UART1_WriteChar(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) {
    }

    USART1->DR = (uint16_t)c;
}

static void UART1_WriteString(const char *str)
{
    while (*str) {
        UART1_WriteChar(*str++);
    }
}

static void UART1_WriteInt(int value)
{
    char buffer[16];

    snprintf(buffer, sizeof(buffer), "%d", value);

    UART1_WriteString(buffer);
}

static void UART1_WriteFloat(float value)
{
    if (value < 0.0f) {
        UART1_WriteChar('-');
        value = -value;
    }

    int whole = (int)value;

    int decimal =
        (int)((value - (float)whole) * 10.0f);

    if (decimal < 0) {
        decimal = -decimal;
    }

    UART1_WriteInt(whole);

    UART1_WriteChar('.');

    UART1_WriteChar(
        (char)('0' + decimal)
    );
}

// ============================================================
// DHT22
// PB0
// ============================================================

static void DHT22_Pin_Output(void)
{
    GPIOB->CRL &= ~(0xFU << 0);
    GPIOB->CRL |=  (0x2U << 0);
}

static void DHT22_Pin_Input(void)
{
    GPIOB->CRL &= ~(0xFU << 0);
    GPIOB->CRL |=  (0x8U << 0);

    GPIOB->ODR |= (1U << 0);
}

static bool DHT22_WaitForLevel(
    bool level,
    uint32_t timeout_us)
{
    while (((GPIOB->IDR & (1U << 0)) != 0) != level) {

        if (timeout_us == 0) {
            return false;
        }

        delay_us(1);
        timeout_us--;
    }

    return true;
}

static bool DHT22_Read(
    float *temperature,
    float *humidity)
{
    uint8_t data[5] = {
        0, 0, 0, 0, 0
    };

    // Start signal
    DHT22_Pin_Output();

    GPIOB->BRR = (1U << 0);

    delay_us(1200);

    GPIOB->BSRR = (1U << 0);

    delay_us(30);

    DHT22_Pin_Input();

    // Sensor response
    if (!DHT22_WaitForLevel(false, 100)) {
        return false;
    }

    if (!DHT22_WaitForLevel(true, 100)) {
        return false;
    }

    if (!DHT22_WaitForLevel(false, 100)) {
        return false;
    }

    // Read 40 bits
    for (int i = 0; i < 40; i++) {

        if (!DHT22_WaitForLevel(true, 100)) {
            return false;
        }

        delay_us(40);

        bool bit =
            (GPIOB->IDR & (1U << 0)) != 0;

        data[i / 8] <<= 1;

        if (bit) {
            data[i / 8] |= 1;
        }

        if (!DHT22_WaitForLevel(false, 100)) {
            return false;
        }
    }

    // Checksum
    uint8_t checksum =
        (uint8_t)(
            data[0] +
            data[1] +
            data[2] +
            data[3]
        );

    if (checksum != data[4]) {
        return false;
    }

    uint16_t rawHumidity =
        ((uint16_t)data[0] << 8) |
        data[1];

    uint16_t rawTemperature =
        ((uint16_t)(data[2] & 0x7F) << 8) |
        data[3];

    *humidity =
        rawHumidity / 10.0f;

    *temperature =
        rawTemperature / 10.0f;

    if (data[2] & 0x80) {
        *temperature =
            -*temperature;
    }

    return true;
}

// ============================================================
// ADC1
// PA0 = LDR
// ============================================================

static void ADC1_Init(void)
{
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_ADC1EN;

    // PA0 = Analog input
    GPIOA->CRL &= ~(0xFU << 0);

    // ADC clock = 8 MHz / 2
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV2;

    ADC1->CR1 = 0;

    // Longest sample time
    ADC1->SMPR2 &= ~(7U << 0);
    ADC1->SMPR2 |=  (7U << 0);

    ADC1->SQR1 = 0;
    ADC1->SQR2 = 0;
    ADC1->SQR3 = 0;

    ADC1->CR2 =
        ADC_CR2_EXTTRIG |
        ADC_CR2_EXTSEL;

    ADC1->CR2 |= ADC_CR2_ADON;

    delay_us(20);

    // Calibration reset
    ADC1->CR2 |= ADC_CR2_RSTCAL;

    while (ADC1->CR2 &
           ADC_CR2_RSTCAL) {
    }

    // Calibration
    ADC1->CR2 |= ADC_CR2_CAL;

    while (ADC1->CR2 &
           ADC_CR2_CAL) {
    }
}

static uint16_t ADC1_Read(void)
{
    ADC1->SQR3 = 0;

    ADC1->CR2 |= ADC_CR2_SWSTART;

    while (!(ADC1->SR & ADC_SR_EOC)) {
    }

    return (uint16_t)ADC1->DR;
}

static int LDR_ReadPercent(void)
{
    uint16_t raw = ADC1_Read();

    int percent =
        (raw * 100) / 4095;

    if (percent < 0) {
        percent = 0;
    }

    if (percent > 100) {
        percent = 100;
    }

    return percent;
}

// ============================================================
// ROTARY ENCODER
// PA1 = CLK
// PA2 = DT
// PA3 = SW
// ============================================================

static void Encoder_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA1, PA2, PA3 = Input Pull-Up
    GPIOA->CRL &= ~(
        (0xFU << 4) |
        (0xFU << 8) |
        (0xFU << 12)
    );

    GPIOA->CRL |=
        (0x8U << 4) |
        (0x8U << 8) |
        (0x8U << 12);

    // Enable internal pull-ups
    GPIOA->ODR |=
        (1U << 1) |
        (1U << 2) |
        (1U << 3);
}

// ============================================================
// PIR MOTION SENSOR
// PA4 = PIR OUT
// ============================================================

static void PIR_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA4 = Input Floating
    GPIOA->CRL &= ~(0xFU << 16);
    GPIOA->CRL |=  (0x4U << 16);
}

static bool PIR_Read(void)
{
    return (GPIOA->IDR & (1U << 4)) != 0;
}

// ============================================================
// SECTION 29
// DISPLAY MODE NAVIGATION
// ============================================================

static DisplayMode NextDisplayMode(
    DisplayMode mode)
{
    switch (mode) {

        case DisplayMode::TEMPERATURE:
            return DisplayMode::HUMIDITY;

        case DisplayMode::HUMIDITY:
            return DisplayMode::LIGHT;

        case DisplayMode::LIGHT:
            return DisplayMode::MOTION;

        case DisplayMode::MOTION:
            return DisplayMode::TEMPERATURE;
    }

    return DisplayMode::TEMPERATURE;
}

static DisplayMode PreviousDisplayMode(
    DisplayMode mode)
{
    switch (mode) {

        case DisplayMode::TEMPERATURE:
            return DisplayMode::MOTION;

        case DisplayMode::HUMIDITY:
            return DisplayMode::TEMPERATURE;

        case DisplayMode::LIGHT:
            return DisplayMode::HUMIDITY;

        case DisplayMode::MOTION:
            return DisplayMode::LIGHT;
    }

    return DisplayMode::TEMPERATURE;
}

static const char *DisplayModeName(
    DisplayMode mode)
{
    switch (mode) {

        case DisplayMode::TEMPERATURE:
            return "TEMPERATURE";

        case DisplayMode::HUMIDITY:
            return "HUMIDITY";

        case DisplayMode::LIGHT:
            return "LIGHT";

        case DisplayMode::MOTION:
            return "MOTION";
    }

    return "UNKNOWN";
}

// ============================================================
// I2C1
// PB6 = SCL
// PB7 = SDA
// ============================================================

#define I2C_TIMEOUT 100000UL
#define OLED_ADDRESS 0x3C

static bool I2C1_Start(void)
{
    uint32_t timeout =
        I2C_TIMEOUT;

    I2C1->CR1 |= I2C_CR1_START;

    while (!(I2C1->SR1 &
             I2C_SR1_SB)) {

        if (--timeout == 0) {
            return false;
        }
    }

    return true;
}

static bool I2C1_SendAddress(uint8_t address)
{
    uint32_t timeout =
        I2C_TIMEOUT;

    I2C1->DR =
        (uint8_t)(address << 1);

    while (!(I2C1->SR1 &
             (I2C_SR1_ADDR |
              I2C_SR1_AF))) {

        if (--timeout == 0) {
            return false;
        }
    }

    if (I2C1->SR1 &
        I2C_SR1_AF) {

        I2C1->SR1 &=
            ~I2C_SR1_AF;

        return false;
    }

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    return true;
}

static bool I2C1_WriteBytes(
    uint8_t address,
    const uint8_t *data,
    uint16_t length)
{
    if (length == 0) {
        return false;
    }

    if (I2C1->SR2 &
        I2C_SR2_BUSY) {

        return false;
    }

    if (!I2C1_Start()) {

        I2C1->CR1 |=
            I2C_CR1_STOP;

        return false;
    }

    if (!I2C1_SendAddress(address)) {

        I2C1->CR1 |=
            I2C_CR1_STOP;

        return false;
    }

    for (uint16_t i = 0;
         i < length;
         i++) {

        uint32_t timeout =
            I2C_TIMEOUT;

        while (!(I2C1->SR1 &
                 I2C_SR1_TXE)) {

            if (I2C1->SR1 &
                I2C_SR1_AF) {

                I2C1->SR1 &=
                    ~I2C_SR1_AF;

                I2C1->CR1 |=
                    I2C_CR1_STOP;

                return false;
            }

            if (--timeout == 0) {

                I2C1->CR1 |=
                    I2C_CR1_STOP;

                return false;
            }
        }

        I2C1->DR = data[i];
    }

    uint32_t timeout =
        I2C_TIMEOUT;

    while (!(I2C1->SR1 &
             I2C_SR1_BTF)) {

        if (--timeout == 0) {

            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    I2C1->CR1 |=
        I2C_CR1_STOP;

    return true;
}

static void I2C1_Init(void)
{
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPBEN |
        RCC_APB2ENR_AFIOEN;

    RCC->APB1ENR |=
        RCC_APB1ENR_I2C1EN;

    // PB6 / PB7 = AF Open Drain 50 MHz
    GPIOB->CRL &= ~(
        (0xFU << 24) |
        (0xFU << 28)
    );

    GPIOB->CRL |=
        (0xBU << 24) |
        (0xBU << 28);

    GPIOB->ODR |=
        (1U << 6) |
        (1U << 7);

    // Reset I2C
    I2C1->CR1 =
        I2C_CR1_SWRST;

    I2C1->CR1 = 0;

    // APB1 = 8 MHz
    I2C1->CR2 = 8;

    // 100 kHz
    I2C1->CCR = 40;

    I2C1->TRISE = 9;

    I2C1->OAR1 = 0x4000;

    I2C1->CR1 =
        I2C_CR1_PE;
}

// ============================================================
// I2C SCANNER
// ============================================================

static bool I2C1_DevicePresent(
    uint8_t address)
{
    uint32_t timeout =
        I2C_TIMEOUT;

    if (I2C1->SR2 &
        I2C_SR2_BUSY) {

        return false;
    }

    I2C1->CR1 |=
        I2C_CR1_START;

    while (!(I2C1->SR1 &
             I2C_SR1_SB)) {

        if (--timeout == 0) {

            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    I2C1->DR =
        (uint8_t)(address << 1);

    timeout =
        I2C_TIMEOUT;

    while (!(I2C1->SR1 &
             (I2C_SR1_ADDR |
              I2C_SR1_AF))) {

        if (--timeout == 0) {

            I2C1->CR1 |=
                I2C_CR1_STOP;

            return false;
        }
    }

    bool found = false;

    if (I2C1->SR1 &
        I2C_SR1_ADDR) {

        (void)I2C1->SR1;
        (void)I2C1->SR2;

        found = true;

    } else {

        I2C1->SR1 &=
            ~I2C_SR1_AF;
    }

    I2C1->CR1 |=
        I2C_CR1_STOP;

    return found;
}

static void I2C1_Scan(void)
{
    UART1_WriteString(
        "I2C SCAN START\r\n"
    );

    const char hex[] =
        "0123456789ABCDEF";

    for (uint8_t address = 1;
         address < 127;
         address++) {

        if (I2C1_DevicePresent(address)) {

            UART1_WriteString(
                "FOUND: 0x"
            );

            UART1_WriteChar(
                hex[(address >> 4) & 0x0F]
            );

            UART1_WriteChar(
                hex[address & 0x0F]
            );

            UART1_WriteString(
                "\r\n"
            );
        }
    }

    UART1_WriteString(
        "I2C SCAN END\r\n\r\n"
    );
}

// ============================================================
// SSD1306 OLED
// ============================================================

static uint8_t oledBuffer[1024];

static bool OLED_Write(
    uint8_t control,
    const uint8_t *data,
    uint16_t length)
{
    uint8_t buffer[17];

    uint16_t offset = 0;

    while (offset < length) {

        uint16_t chunk =
            length - offset;

        if (chunk > 16) {
            chunk = 16;
        }

        buffer[0] = control;

        for (uint16_t i = 0;
             i < chunk;
             i++) {

            buffer[i + 1] =
                data[offset + i];
        }

        if (!I2C1_WriteBytes(
                OLED_ADDRESS,
                buffer,
                (uint16_t)(chunk + 1))) {

            return false;
        }

        offset += chunk;
    }

    return true;
}

static bool OLED_Command(
    uint8_t command)
{
    return OLED_Write(
        0x00,
        &command,
        1
    );
}

static bool OLED_Command2(
    uint8_t command,
    uint8_t parameter)
{
    uint8_t data[2];

    data[0] = command;
    data[1] = parameter;

    return OLED_Write(
        0x00,
        data,
        2
    );
}

// ============================================================
// OLED INITIALIZATION
// ============================================================

static bool OLED_Init(void)
{
    bool ok = true;

    delay_ms_blocking(100);

    ok &= OLED_Command(0xAE);

    ok &= OLED_Command2(
        0xD5,
        0x80
    );

    ok &= OLED_Command2(
        0xA8,
        0x3F
    );

    ok &= OLED_Command2(
        0xD3,
        0x00
    );

    ok &= OLED_Command(
        0x40
    );

    ok &= OLED_Command2(
        0x8D,
        0x14
    );

    ok &= OLED_Command2(
        0x20,
        0x02
    );

    // NORMAL ORIENTATION
    ok &= OLED_Command(
        0xA0
    );

    ok &= OLED_Command(
        0xC0
    );

    ok &= OLED_Command2(
        0xDA,
        0x12
    );

    ok &= OLED_Command2(
        0x81,
        0x7F
    );

    ok &= OLED_Command2(
        0xD9,
        0xF1
    );

    ok &= OLED_Command2(
        0xDB,
        0x40
    );

    ok &= OLED_Command(
        0xA4
    );

    ok &= OLED_Command(
        0xA6
    );

    ok &= OLED_Command(
        0xAF
    );

    return ok;
}

// ============================================================
// OLED UPDATE
// ============================================================

static bool OLED_Update(void)
{
    for (uint8_t page = 0;
         page < 8;
         page++) {

        if (!OLED_Command(
                (uint8_t)(0xB0 | page))) {

            return false;
        }

        if (!OLED_Command(
                0x00)) {

            return false;
        }

        if (!OLED_Command(
                0x10)) {

            return false;
        }

        if (!OLED_Write(
                0x40,
                &oledBuffer[
                    (uint16_t)page * 128
                ],
                128)) {

            return false;
        }
    }

    return true;
}

static void OLED_ClearBuffer(void)
{
    for (uint16_t i = 0;
         i < 1024;
         i++) {

        oledBuffer[i] = 0x00;
    }
}

// ============================================================
// 5x7 FONT
// ============================================================

static void OLED_DrawChar(
    uint8_t x,
    uint8_t page,
    char c)
{
    if (x > 122 ||
        page > 7) {

        return;
    }

    uint8_t glyph[5] = {
        0, 0, 0, 0, 0
    };

    switch (c) {

        case 'A':
            glyph[0] = 0x7E;
            glyph[1] = 0x09;
            glyph[2] = 0x09;
            glyph[3] = 0x09;
            glyph[4] = 0x7E;
            break;

        case 'C':
            glyph[0] = 0x3E;
            glyph[1] = 0x41;
            glyph[2] = 0x41;
            glyph[3] = 0x41;
            glyph[4] = 0x22;
            break;

        case 'E':
            glyph[0] = 0x7F;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x41;
            break;

        case 'G':
            glyph[0] = 0x3E;
            glyph[1] = 0x41;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x7A;
            break;

        case 'H':
            glyph[0] = 0x7F;
            glyph[1] = 0x08;
            glyph[2] = 0x08;
            glyph[3] = 0x08;
            glyph[4] = 0x7F;
            break;

        case 'I':
            glyph[0] = 0x00;
            glyph[1] = 0x41;
            glyph[2] = 0x7F;
            glyph[3] = 0x41;
            glyph[4] = 0x00;
            break;

        case 'L':
            glyph[0] = 0x7F;
            glyph[1] = 0x40;
            glyph[2] = 0x40;
            glyph[3] = 0x40;
            glyph[4] = 0x40;
            break;

        case 'M':
            glyph[0] = 0x7F;
            glyph[1] = 0x02;
            glyph[2] = 0x0C;
            glyph[3] = 0x02;
            glyph[4] = 0x7F;
            break;

        case 'N':
            glyph[0] = 0x7F;
            glyph[1] = 0x04;
            glyph[2] = 0x08;
            glyph[3] = 0x10;
            glyph[4] = 0x7F;
            break;

        case 'O':
            glyph[0] = 0x3E;
            glyph[1] = 0x41;
            glyph[2] = 0x41;
            glyph[3] = 0x41;
            glyph[4] = 0x3E;
            break;

        case 'P':
            glyph[0] = 0x7F;
            glyph[1] = 0x09;
            glyph[2] = 0x09;
            glyph[3] = 0x09;
            glyph[4] = 0x06;
            break;

        case 'R':
            glyph[0] = 0x7F;
            glyph[1] = 0x09;
            glyph[2] = 0x19;
            glyph[3] = 0x29;
            glyph[4] = 0x46;
            break;

        case 'T':
            glyph[0] = 0x01;
            glyph[1] = 0x01;
            glyph[2] = 0x7F;
            glyph[3] = 0x01;
            glyph[4] = 0x01;
            break;

        case 'U':
            glyph[0] = 0x3F;
            glyph[1] = 0x40;
            glyph[2] = 0x40;
            glyph[3] = 0x40;
            glyph[4] = 0x3F;
            break;

        case '0':
            glyph[0] = 0x3E;
            glyph[1] = 0x45;
            glyph[2] = 0x49;
            glyph[3] = 0x51;
            glyph[4] = 0x3E;
            break;

        case '1':
            glyph[0] = 0x00;
            glyph[1] = 0x21;
            glyph[2] = 0x7F;
            glyph[3] = 0x01;
            glyph[4] = 0x00;
            break;

        case '2':
            glyph[0] = 0x23;
            glyph[1] = 0x45;
            glyph[2] = 0x49;
            glyph[3] = 0x51;
            glyph[4] = 0x21;
            break;

        case '3':
            glyph[0] = 0x22;
            glyph[1] = 0x41;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x36;
            break;

        case '4':
            glyph[0] = 0x0C;
            glyph[1] = 0x14;
            glyph[2] = 0x24;
            glyph[3] = 0x7F;
            glyph[4] = 0x04;
            break;

        case '5':
            glyph[0] = 0x72;
            glyph[1] = 0x51;
            glyph[2] = 0x51;
            glyph[3] = 0x51;
            glyph[4] = 0x4E;
            break;

        case '6':
            glyph[0] = 0x1E;
            glyph[1] = 0x29;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x06;
            break;

        case '7':
            glyph[0] = 0x40;
            glyph[1] = 0x47;
            glyph[2] = 0x48;
            glyph[3] = 0x50;
            glyph[4] = 0x60;
            break;

        case '8':
            glyph[0] = 0x36;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x36;
            break;

        case '9':
            glyph[0] = 0x30;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x4A;
            glyph[4] = 0x3C;
            break;

        case '.':
            glyph[0] = 0x00;
            glyph[1] = 0x60;
            glyph[2] = 0x60;
            glyph[3] = 0x00;
            glyph[4] = 0x00;
            break;

        case '%':
            glyph[0] = 0x63;
            glyph[1] = 0x13;
            glyph[2] = 0x08;
            glyph[3] = 0x64;
            glyph[4] = 0x63;
            break;

        case ' ':
        default:
            break;
    }

    uint16_t index =
        (uint16_t)page * 128 + x;

    for (uint8_t i = 0;
         i < 5;
         i++) {

        if ((x + i) < 128) {

            oledBuffer[
                index + i
            ] = glyph[i];
        }
    }

    if ((x + 5) < 128) {

        oledBuffer[
            index + 5
        ] = 0x00;
    }
}

static void OLED_DrawString(
    uint8_t x,
    uint8_t page,
    const char *text)
{
    while (*text &&
           x <= 122) {

        OLED_DrawChar(
            x,
            page,
            *text
        );

        x += 6;
        text++;
    }
}

// ============================================================
// ROTATED VALUE CHARACTER
// TRUE 180-DEGREE ROTATION
// ============================================================

static void OLED_DrawCharRotated180(
    uint8_t x,
    uint8_t page,
    char c)
{
    if (x > 122 ||
        page > 7) {

        return;
    }

    uint8_t glyph[5] = {
        0, 0, 0, 0, 0
    };

    switch (c) {

        case '0':
            glyph[0] = 0x3E;
            glyph[1] = 0x45;
            glyph[2] = 0x49;
            glyph[3] = 0x51;
            glyph[4] = 0x3E;
            break;

        case '1':
            glyph[0] = 0x00;
            glyph[1] = 0x21;
            glyph[2] = 0x7F;
            glyph[3] = 0x01;
            glyph[4] = 0x00;
            break;

        case '2':
            glyph[0] = 0x23;
            glyph[1] = 0x45;
            glyph[2] = 0x49;
            glyph[3] = 0x51;
            glyph[4] = 0x21;
            break;

        case '3':
            glyph[0] = 0x22;
            glyph[1] = 0x41;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x36;
            break;

        case '4':
            glyph[0] = 0x0C;
            glyph[1] = 0x14;
            glyph[2] = 0x24;
            glyph[3] = 0x7F;
            glyph[4] = 0x04;
            break;

        case '5':
            glyph[0] = 0x72;
            glyph[1] = 0x51;
            glyph[2] = 0x51;
            glyph[3] = 0x51;
            glyph[4] = 0x4E;
            break;

        case '6':
            glyph[0] = 0x1E;
            glyph[1] = 0x29;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x06;
            break;

        case '7':
            glyph[0] = 0x40;
            glyph[1] = 0x47;
            glyph[2] = 0x48;
            glyph[3] = 0x50;
            glyph[4] = 0x60;
            break;

        case '8':
            glyph[0] = 0x36;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x36;
            break;

        case '9':
            glyph[0] = 0x30;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x4A;
            glyph[4] = 0x3C;
            break;

        case '.':
            glyph[0] = 0x00;
            glyph[1] = 0x60;
            glyph[2] = 0x60;
            glyph[3] = 0x00;
            glyph[4] = 0x00;
            break;

        case '%':
            glyph[0] = 0x63;
            glyph[1] = 0x13;
            glyph[2] = 0x08;
            glyph[3] = 0x64;
            glyph[4] = 0x63;
            break;

        case 'C':
            glyph[0] = 0x3E;
            glyph[1] = 0x41;
            glyph[2] = 0x41;
            glyph[3] = 0x41;
            glyph[4] = 0x22;
            break;

        case ' ':
        default:
            break;
    }

    uint16_t index =
        (uint16_t)page * 128 + x;

    for (uint8_t col = 0;
         col < 5;
         col++) {

        uint8_t rotated = 0;

        for (uint8_t row = 0;
             row < 7;
             row++) {

            if (glyph[col] &
                (1U << row)) {

                rotated |=
                    (uint8_t)(1U << (6 - row));
            }
        }

        oledBuffer[
            index + (4 - col)
        ] = rotated;
    }

    if ((x + 5) < 128) {
        oledBuffer[index + 5] = 0x00;
    }
}

// ============================================================
// ROTATED VALUE STRING
// TRUE 180-DEGREE ROTATION
// ============================================================

static void OLED_DrawStringRotated180(
    uint8_t x,
    uint8_t page,
    const char *text)
{
    uint8_t length = 0;

    while (text[length] != '\0') {
        length++;
    }

    // True 180-degree rotation also reverses
    // the order of the characters.
    for (uint8_t i = 0;
         i < length;
         i++) {

        uint8_t drawX =
            x + (uint8_t)(
                (length - 1 - i) * 6
            );

        OLED_DrawCharRotated180(
            drawX,
            page,
            text[i]
        );
    }
}

// ============================================================
// ROOM MONITOR DISPLAY
// ============================================================

static void OLED_ShowRoomMonitor(
    float temperature,
    float humidity,
    int lightLevel)
{
    char tempText[16];
    char humidityText[16];
    char lightText[16];

    int tempWhole =
        (int)temperature;

    int tempDecimal =
        (int)(
            (temperature -
             (float)tempWhole) *
            10.0f
        );

    if (tempDecimal < 0) {
        tempDecimal =
            -tempDecimal;
    }

    snprintf(
        tempText,
        sizeof(tempText),
        "%d.%d C",
        tempWhole,
        tempDecimal
    );

    int humidityWhole =
        (int)humidity;

    int humidityDecimal =
        (int)(
            (humidity -
             (float)humidityWhole) *
            10.0f
        );

    if (humidityDecimal < 0) {
        humidityDecimal =
            -humidityDecimal;
    }

    snprintf(
        humidityText,
        sizeof(humidityText),
        "%d.%d%%",
        humidityWhole,
        humidityDecimal
    );

    snprintf(
        lightText,
        sizeof(lightText),
        "%d%%",
        lightLevel
    );

    OLED_ClearBuffer();

    // Header - NORMAL
    OLED_DrawString(
        0,
        0,
        "ROOM MONITOR"
    );

    // Temperature label - NORMAL
    OLED_DrawString(
        0,
        2,
        "TEMP"
    );

    // Temperature value - ROTATED 180
    OLED_DrawStringRotated180(
        48,
        2,
        tempText
    );

    // Humidity label - NORMAL
    OLED_DrawString(
        0,
        4,
        "HUM"
    );

    // Humidity value - ROTATED 180
    OLED_DrawStringRotated180(
        48,
        4,
        humidityText
    );

    // Light label - NORMAL
    OLED_DrawString(
        0,
        6,
        "LIGHT"
    );

    // Light value - ROTATED 180
    OLED_DrawStringRotated180(
        48,
        6,
        lightText
    );

    if (!OLED_Update()) {

        UART1_WriteString(
            "OLED UPDATE FAILED\r\n"
        );
    }
}

// ============================================================
// SENSOR TASK
// Priority = 2
// Period = 2 seconds
// ============================================================

static void SensorTask(
    void *argument)
{
    (void)argument;

    TickType_t lastWakeTime =
        xTaskGetTickCount();

    for (;;) {

        SensorData data;

        data.temperature = 0.0f;
        data.humidity = 0.0f;
        data.lightLevel = 0;
        data.motionDetected = false;

        // Read DHT22
        if (!DHT22_Read(
                &data.temperature,
                &data.humidity)) {

            UART1_WriteString(
                "DHT22 read failed\r\n"
            );
        }

        // Read LDR
        data.lightLevel =
            LDR_ReadPercent();

        // Motion will be handled by MotionTask.
        data.motionDetected = false;

        // Send newest sensor data
        if (sensorQueue != NULL) {

            if (xQueueOverwrite(
                    sensorQueue,
                    &data) != pdPASS) {

                UART1_WriteString(
                    "Queue send failed\r\n"
                );
            }
        }

        // Required periodic timing
        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}

// ============================================================
// INPUT TASK
// Priority = 3
//
// SECTION 29 NAVIGATION
//
// Clockwise:
// TEMPERATURE -> HUMIDITY -> LIGHT -> MOTION
// -> TEMPERATURE
//
// Counterclockwise:
// TEMPERATURE -> MOTION -> LIGHT -> HUMIDITY
// -> TEMPERATURE
// ============================================================

static void InputTask(
    void *argument)
{
    (void)argument;

    uint8_t previousCLK =
        (GPIOA->IDR & (1U << 1)) ? 1 : 0;

    for (;;) {

        uint8_t currentCLK =
            (GPIOA->IDR & (1U << 1)) ? 1 : 0;

        uint8_t currentDT =
            (GPIOA->IDR & (1U << 2)) ? 1 : 0;

        // Detect falling edge on CLK.
        if (previousCLK == 1 &&
            currentCLK == 0) {

            if (currentDT == 1) {

                // CLOCKWISE
                currentDisplayMode =
                    NextDisplayMode(
                        currentDisplayMode
                    );

                UART1_WriteString(
                    "Encoder Clockwise -> "
                );

                UART1_WriteString(
                    DisplayModeName(
                        currentDisplayMode
                    )
                );

                UART1_WriteString(
                    "\r\n"
                );

            } else {

                // COUNTERCLOCKWISE
                currentDisplayMode =
                    PreviousDisplayMode(
                        currentDisplayMode
                    );

                UART1_WriteString(
                    "Encoder Counterclockwise -> "
                );

                UART1_WriteString(
                    DisplayModeName(
                        currentDisplayMode
                    )
                );

                UART1_WriteString(
                    "\r\n"
                );
            }
        }

        previousCLK = currentCLK;

        // Poll encoder every 10 ms
        vTaskDelay(
            pdMS_TO_TICKS(10)
        );
    }
}

// ============================================================
// MOTION TASK
// Priority = 3
// SECTION 31
//
// PIR is monitored continuously.
// PA4 HIGH = motion detected.
// ============================================================

static void MotionTask(
    void *argument)
{
    (void)argument;

    for (;;) {

        bool motionDetected =
            PIR_Read();

        if (motionDetected) {

            UART1_WriteString(
                "Motion detected\r\n"
            );
        }

        // Poll PIR every 100 ms
        vTaskDelay(
            pdMS_TO_TICKS(100)
        );
    }
}

// ============================================================
// DISPLAY TASK
// Priority = 1
// ============================================================

static void DisplayTask(
    void *argument)
{
    (void)argument;

    SensorData data;

    for (;;) {

        if (xQueueReceive(
                sensorQueue,
                &data,
                portMAX_DELAY) == pdPASS) {

            // Serial monitor
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

            UART1_WriteString(
                "Light: "
            );

            UART1_WriteInt(
                data.lightLevel
            );

            UART1_WriteString(
                " %\r\n"
            );

            // ====================================================
            // SECTION 30
            // Evaluate temperature alarm state.
            // Hardware buzzer control is intentionally separate.
            // ====================================================

            AlarmState alarmState =
                evaluateTemperature(
                    data.temperature
                );

            if (alarmState ==
                AlarmState::LOW_TEMPERATURE) {

                UART1_WriteString(
                    "Alarm State: LOW TEMPERATURE\r\n"
                );

            } else if (
                alarmState ==
                AlarmState::HIGH_TEMPERATURE) {

                UART1_WriteString(
                    "Alarm State: HIGH TEMPERATURE\r\n"
                );

            } else {

                UART1_WriteString(
                    "Alarm State: NORMAL\r\n"
                );
            }

            UART1_WriteString(
                "--------------------\r\n"
            );

            // OLED remains owned by DisplayTask.
            // Actual page rendering will be added
            // when the following lab section requires it.
            OLED_ShowRoomMonitor(
                data.temperature,
                data.humidity,
                data.lightLevel
            );
        }
    }
}

// ============================================================
// MAIN
// ============================================================

int main(void)
{
    DWT_Init();

    UART1_Init();

    // ========================================================
    // STARTUP MESSAGE
    // ========================================================

    UART1_WriteString(
        "\r\nBCA182 FreeRTOS Multisensor\r\n"
    );

    UART1_WriteString(
        "System starting...\r\n"
    );

    // Enable GPIOB clock
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPBEN;

    // DHT22
    DHT22_Pin_Input();

    // ADC / LDR
    ADC1_Init();

    // Rotary encoder
    Encoder_Init();

    // PIR motion sensor
    PIR_Init();

    // I2C / OLED
    I2C1_Init();

    UART1_WriteString(
        "I2C SCAN START\r\n"
    );

    I2C1_Scan();

    UART1_WriteString(
        "Initializing OLED...\r\n"
    );

    if (OLED_Init()) {

        UART1_WriteString(
            "OLED initialization complete.\r\n"
        );

    } else {

        UART1_WriteString(
            "OLED initialization FAILED.\r\n"
        );
    }

    // Initial screen
    OLED_ShowRoomMonitor(
        25.4f,
        62.5f,
        24
    );

    UART1_WriteString(
        "Initial OLED screen sent.\r\n"
    );

    // ========================================================
    // SENSOR QUEUE
    // ========================================================

    sensorQueue =
        xQueueCreate(
            1,
            sizeof(SensorData)
        );

    if (sensorQueue == NULL) {

        UART1_WriteString(
            "ERROR: Queue creation failed!\r\n"
        );

        while (1) {
        }
    }

    // ========================================================
    // SENSOR TASK
    // ========================================================

    if (xTaskCreate(
            SensorTask,
            "SensorTask",
            256,
            NULL,
            2,
            NULL
        ) != pdPASS) {

        UART1_WriteString(
            "ERROR: SensorTask creation failed!\r\n"
        );

        while (1) {
        }
    }

    // ========================================================
    // DISPLAY TASK
    // ========================================================

    if (xTaskCreate(
            DisplayTask,
            "DisplayTask",
            512,
            NULL,
            1,
            NULL
        ) != pdPASS) {

        UART1_WriteString(
            "ERROR: DisplayTask creation failed!\r\n"
        );

        while (1) {
        }
    }

    // ========================================================
    // INPUT TASK
    // ========================================================

    if (xTaskCreate(
            InputTask,
            "InputTask",
            256,
            NULL,
            3,
            NULL
        ) != pdPASS) {

        UART1_WriteString(
            "ERROR: InputTask creation failed!\r\n"
        );

        while (1) {
        }
    }

    // ========================================================
    // MOTION TASK
    // ========================================================

    if (xTaskCreate(
            MotionTask,
            "MotionTask",
            256,
            NULL,
            3,
            NULL
        ) != pdPASS) {

        UART1_WriteString(
            "ERROR: MotionTask creation failed!\r\n"
        );

        while (1) {
        }
    }

    // ========================================================
    // START FREERTOS
    // ========================================================

    UART1_WriteString(
        "Starting FreeRTOS scheduler...\r\n"
    );

    vTaskStartScheduler();

    // Should never reach here
    while (1) {
    }
}