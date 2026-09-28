#ifndef SERIAL_MUTEX_H
#define SERIAL_MUTEX_H

void SerialMutex_Init();

void Serial_WriteString(
    const char *text
);

void Serial_WriteInt(
    int value
);

void Serial_WriteFloat(
    float value
);

#endif