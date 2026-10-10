#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

// ==================================================================
//  Shared display object
// ==================================================================
// Defined in display_ui.cpp. Other modules (fingerprint_module,
// rfid_module, touch_input via app_state) only call the draw
// functions below rather than touching tft directly, but it's
// exposed here in case low-level access is ever needed.
extern TFT_eSPI tft;

// ==================================================================
//  Screen draw functions
// ==================================================================
void drawHomeScreen();
void drawIdleScreen();
void drawCapturingScreen();
void drawResultScreen(bool success, String idLabel, int confidence, const char* note);
void drawBanner(String msg, uint16_t color);
void drawCommandScreen(String msg, uint16_t color);

// ==================================================================
//  UI primitives
// ==================================================================
void drawMenuButton();
void drawToggleButton();
void drawButton(Btn b);
void drawTouchMarker(int x, int y);

#endif // DISPLAY_UI_H