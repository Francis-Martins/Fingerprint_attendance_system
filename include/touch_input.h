#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ==================================================================
//  Shared touchscreen objects
// ==================================================================
// Defined in touch_input.cpp. main.cpp needs these directly for the
// setup()-time touchscreenSPI.begin()/touchscreen.begin() calls.
extern SPIClass touchscreenSPI;
extern XPT2046_Touchscreen touchscreen;

// ==================================================================
//  Touch polling — called every loop() regardless of appMode
// ==================================================================
void handleTouch();

#endif // TOUCH_INPUT_H