#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdbool.h>
#include <stdint.h>

// UART

void UART1_Init();

void UART1_WriteChar(char c);

void UART1_WriteString(const char *str);

void UART1_WriteInt(int value);

void UART1_WriteFloat(float value);

// Timing

void DWT_Init();

void delay_us(uint32_t us);

void delay_ms_blocking(uint32_t ms);

// DHT22

void DHT22_Pin_Input();

bool DHT22_Read(float *temperature, float *humidity);

// LDR

void ADC1_Init();

int LDR_ReadPercent();

// Encoder

void Encoder_Init();

bool Encoder_ButtonPressed();

int Encoder_ReadStep();

// PIR

void PIR_Init();

bool PIR_Read();

// I2C

void I2C1_Init();

void I2C1_Scan();

bool I2C1_WriteBytes(
    uint8_t address,
    const uint8_t *data,
    uint16_t length
);

#endif