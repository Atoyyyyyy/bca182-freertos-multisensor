#ifndef INPUT_H
#define INPUT_H

void Encoder_Init();
int Encoder_ReadStep();
bool Encoder_ButtonPressed();

void InputTask(void *argument);

#endif