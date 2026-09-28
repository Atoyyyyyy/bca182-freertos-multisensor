#include "stm32f1xx.h"

#include <stdint.h>
#include <stdbool.h>

#include "hardware.h"
#include "oled.h"

#define OLED_ADDRESS       0x3C
#define OLED_WRITE_ADDRESS (OLED_ADDRESS << 1)

static uint8_t oledBuffer[1024];

// ============================================================
// LOW-LEVEL OLED I2C WRITE
// ============================================================

static bool OLED_I2C_Write(
    const uint8_t *data,
    uint16_t length)
{
    if (data == nullptr || length == 0) {
        return false;
    }

    bool result =
        I2C1_WriteBytes(
            OLED_ADDRESS,
            data,
            length
        );

    if (!result) {
        UART1_WriteString(
            "OLED I2C WRITE FAILED\r\n"
        );
    }

    return result;
}

// ============================================================
// OLED COMMAND
// ============================================================

static bool OLED_Command(uint8_t command)
{
    uint8_t data[2];

    data[0] = 0x00;
    data[1] = command;

    bool result =
        OLED_I2C_Write(data, 2);

    if (!result) {
        UART1_WriteString(
            "OLED COMMAND FAILED: "
        );

        UART1_WriteInt(
            static_cast<int>(command)
        );

        UART1_WriteString(
            "\r\n"
        );
    }

    return result;
}

// ============================================================
// OLED POWER
//
// 0xAE = display OFF
// 0xAF = display ON
// ============================================================

void OLED_SetPower(bool on)
{
    if (on) {
        OLED_Command(0xAF);
    } else {
        OLED_Command(0xAE);
    }
}

// ============================================================
// OLED DATA
// ============================================================

static bool OLED_WriteData(
    const uint8_t *buffer,
    uint16_t length)
{
    if (buffer == nullptr || length == 0) {
        return false;
    }

    uint8_t packet[17];

    uint16_t offset = 0;

    while (offset < length) {

        uint16_t chunk =
            length - offset;

        if (chunk > 16) {
            chunk = 16;
        }

        packet[0] = 0x40;

        for (uint16_t i = 0; i < chunk; i++) {
            packet[i + 1] =
                buffer[offset + i];
        }

        if (!OLED_I2C_Write(
                packet,
                chunk + 1)) {

            return false;
        }

        offset += chunk;
    }

    return true;
}

// ============================================================
// OLED INITIALIZATION
// ============================================================

bool OLED_Init(void)
{
    delay_ms_blocking(100);

    // Keep OLED OFF during initialization
    if (!OLED_Command(0xAE)) return false;

    if (!OLED_Command(0xD5)) return false;
    if (!OLED_Command(0x80)) return false;

    if (!OLED_Command(0xA8)) return false;
    if (!OLED_Command(0x3F)) return false;

    if (!OLED_Command(0xD3)) return false;
    if (!OLED_Command(0x00)) return false;

    if (!OLED_Command(0x40)) return false;

    if (!OLED_Command(0x8D)) return false;
    if (!OLED_Command(0x14)) return false;

    if (!OLED_Command(0x20)) return false;
    if (!OLED_Command(0x00)) return false;

    // ========================================================
    // ROTATE OLED CONTENT 180 DEGREES
    // ========================================================

    if (!OLED_Command(0xA0)) return false;
    if (!OLED_Command(0xC0)) return false;

    // ========================================================

    if (!OLED_Command(0xDA)) return false;
    if (!OLED_Command(0x12)) return false;

    if (!OLED_Command(0x81)) return false;
    if (!OLED_Command(0x7F)) return false;

    if (!OLED_Command(0xD9)) return false;
    if (!OLED_Command(0xF1)) return false;

    if (!OLED_Command(0xDB)) return false;
    if (!OLED_Command(0x40)) return false;

    if (!OLED_Command(0xA4)) return false;

    if (!OLED_Command(0xA6)) return false;

    // IMPORTANT:
    // Leave OLED OFF at startup.
    if (!OLED_Command(0xAE)) return false;

    OLED_ClearBuffer();

    return OLED_Update();
}

// ============================================================
// CLEAR BUFFER
// ============================================================

void OLED_ClearBuffer(void)
{
    for (uint16_t i = 0; i < 1024; i++) {
        oledBuffer[i] = 0;
    }
}

// ============================================================
// SEND BUFFER TO OLED
// ============================================================

bool OLED_Update(void)
{
    for (uint8_t page = 0; page < 8; page++) {

        if (!OLED_Command(
                static_cast<uint8_t>(
                    0xB0 | page))) {

            return false;
        }

        if (!OLED_Command(0x00)) {
            return false;
        }

        if (!OLED_Command(0x10)) {
            return false;
        }

        if (!OLED_WriteData(
                &oledBuffer[page * 128],
                128)) {

            return false;
        }
    }

    return true;
}

// ============================================================
// 5x7 FONT
// ============================================================

static const uint8_t font5x7[][5] = {

    {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},
    {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},
    {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},
    {0x14,0x08,0x3E,0x08,0x14},
    {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},
    {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},

    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},

    {0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},
    {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},
    {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},

    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}
};

// ============================================================
// DRAW CHARACTER
// ============================================================

static void OLED_DrawChar(
    uint8_t x,
    uint8_t y,
    char character)
{
    if (x >= 128 || y >= 64) {
        return;
    }

    if (character < ' ' || character > 'Z') {
        character = ' ';
    }

    const uint8_t *glyph =
        font5x7[character - ' '];

    for (uint8_t column = 0;
         column < 5;
         column++) {

        uint8_t bits =
            glyph[column];

        for (uint8_t row = 0;
             row < 7;
             row++) {

            uint8_t pixelY =
                y + row;

            uint8_t pixelX =
                x + column;

            if (pixelX >= 128 ||
                pixelY >= 64) {

                continue;
            }

            uint16_t index =
                (pixelY / 8) * 128 +
                pixelX;

            uint8_t mask =
                static_cast<uint8_t>(
                    1U << (pixelY % 8)
                );

            if (bits & (1U << row)) {
                oledBuffer[index] |= mask;
            } else {
                oledBuffer[index] &= ~mask;
            }
        }
    }
}

// ============================================================
// DRAW STRING
// ============================================================

static void OLED_DrawString(
    uint8_t x,
    uint8_t y,
    const char *text)
{
    if (text == nullptr) {
        return;
    }

    while (*text != '\0') {

        if (x > 122) {
            break;
        }

        OLED_DrawChar(
            x,
            y,
            *text
        );

        x += 6;
        text++;
    }
}

// ============================================================
// DRAW INTEGER
// ============================================================

static void OLED_DrawInt(
    uint8_t x,
    uint8_t y,
    int value)
{
    char buffer[12];

    int index = 0;

    if (value == 0) {

        buffer[index++] = '0';

    } else {

        if (value < 0) {

            buffer[index++] = '-';
            value = -value;
        }

        char digits[10];
        int digitCount = 0;

        while (value > 0 &&
               digitCount < 10) {

            digits[digitCount++] =
                static_cast<char>(
                    '0' + (value % 10)
                );

            value /= 10;
        }

        while (digitCount > 0) {

            buffer[index++] =
                digits[--digitCount];
        }
    }

    buffer[index] = '\0';

    OLED_DrawString(
        x,
        y,
        buffer
    );
}

// ============================================================
// DRAW FLOAT
// ============================================================

static void OLED_DrawFloat(
    uint8_t x,
    uint8_t y,
    float value)
{
    if (value < 0.0f) {

        OLED_DrawChar(
            x,
            y,
            '-'
        );

        value = -value;
        x += 6;
    }

    int whole =
        static_cast<int>(value);

    int decimal =
        static_cast<int>(
            (value - whole) * 10.0f + 0.5f
        );

    if (decimal >= 10) {
        decimal = 0;
        whole++;
    }

    OLED_DrawInt(
        x,
        y,
        whole
    );

    int digits = 1;
    int tempValue = whole;

    while (tempValue >= 10) {
        tempValue /= 10;
        digits++;
    }

    x += static_cast<uint8_t>(
        digits * 6
    );

    OLED_DrawChar(
        x,
        y,
        '.'
    );

    OLED_DrawChar(
        x + 6,
        y,
        static_cast<char>(
            '0' + decimal
        )
    );
}

// ============================================================
// TEMPERATURE SCREEN
// ============================================================

void OLED_ShowTemperature(
    float temperature)
{
    OLED_ClearBuffer();

    OLED_DrawString(
        30,
        8,
        "TEMPERATURE"
    );

    OLED_DrawFloat(
        42,
        32,
        temperature
    );

    OLED_DrawString(
        78,
        32,
        "C"
    );

    OLED_Update();
}

// ============================================================
// HUMIDITY SCREEN
// ============================================================

void OLED_ShowHumidity(
    float humidity)
{
    OLED_ClearBuffer();

    OLED_DrawString(
        42,
        8,
        "HUMIDITY"
    );

    OLED_DrawFloat(
        42,
        32,
        humidity
    );

    OLED_DrawString(
        78,
        32,
        "%"
    );

    OLED_Update();
}

// ============================================================
// LIGHT SCREEN
// ============================================================

void OLED_ShowLight(
    int lightLevel)
{
    OLED_ClearBuffer();

    OLED_DrawString(
        48,
        8,
        "LIGHT"
    );

    OLED_DrawInt(
        48,
        32,
        lightLevel
    );

    OLED_DrawString(
        72,
        32,
        "%"
    );

    OLED_Update();
}

// ============================================================
// ROOM MONITOR DISPLAY
// ============================================================

void OLED_ShowRoomMonitor(
    float temperature,
    float humidity,
    int lightLevel)
{
    OLED_ClearBuffer();

    OLED_DrawString(
        30,
        0,
        "ROOM"
    );

    OLED_DrawString(
        60,
        0,
        "MONITOR"
    );

    OLED_DrawString(
        0,
        16,
        "TEMP:"
    );

    OLED_DrawFloat(
        36,
        16,
        temperature
    );

    OLED_DrawString(
        72,
        16,
        "C"
    );

    OLED_DrawString(
        0,
        32,
        "HUM:"
    );

    OLED_DrawFloat(
        30,
        32,
        humidity
    );

    OLED_DrawString(
        66,
        32,
        "%"
    );

    OLED_DrawString(
        0,
        48,
        "LIGHT:"
    );

    OLED_DrawInt(
        42,
        48,
        lightLevel
    );

    OLED_DrawString(
        66,
        48,
        "%"
    );

    OLED_Update();
}