#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

void buzzerBegin();    // call once in setup()
void buzzerStart();    // start a 1s buzz (non-blocking)
void buzzerUpdate();   // call every loop() tick

#endif // BUZZER_H