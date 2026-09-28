#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

void Encoder_Init();
int Encoder_ReadStep();
bool Encoder_ButtonPressed();

#endif