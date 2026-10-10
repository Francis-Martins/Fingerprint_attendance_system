#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"
#include "display_ui.h"
#include "touch_input.h"

#ifdef RUN_CALIBRATION

namespace calib {

const int SCREEN_W = 320;
const int SCREEN_H = 240;
const int INSET = 20;
const int SAMPLES = 20;

int targetX[4] = { INSET, SCREEN_W - INSET, SCREEN_W - INSET, INSET };
int targetY[4] = { INSET, INSET, SCREEN_H - INSET, SCREEN_H - INSET };
const char* targetName[4] = { "TOP-LEFT", "TOP-RIGHT", "BOTTOM-RIGHT", "BOTTOM-LEFT" };

long rawX[4], rawY[4];
int xMin, xMax, yMin, yMax;

void crosshair(int x, int y, uint16_t c) {
  tft.drawLine(x - 10, y, x + 10, y, c);
  tft.drawLine(x, y - 10, x, y + 10, c);
  tft.drawCircle(x, y, 6, c);
}

void waitRelease() {
  while (touchscreen.touched()) delay(10);
  delay(200);
}

bool capture(int i) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(40, 100);
  tft.print("Touch the target");
  tft.setTextSize(1);
  tft.setCursor(40, 130);
  tft.print(targetName[i]);
  crosshair(targetX[i], targetY[i], TFT_RED);

  while (!touchscreen.touched()) delay(10);
  delay(150);

  long sx = 0, sy = 0;
  int got = 0;
  for (int s = 0; s < SAMPLES * 3 && got < SAMPLES; s++) {
    if (touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      sx += p.x; sy += p.y; got++;
    }
    delay(10);
  }
  if (got < SAMPLES / 2) {
    Serial.println("Not enough samples, retrying...");
    waitRelease();
    return false;
  }

  rawX[i] = sx / got;
  rawY[i] = sy / got;
  Serial.printf("%s  raw x=%ld  raw y=%ld\n", targetName[i], rawX[i], rawY[i]);
  crosshair(targetX[i], targetY[i], TFT_GREEN);
  waitRelease();
  return true;
}

void compute() {
  float left   = (rawX[0] + rawX[3]) / 2.0;
  float right  = (rawX[1] + rawX[2]) / 2.0;
  float top    = (rawY[0] + rawY[1]) / 2.0;
  float bottom = (rawY[2] + rawY[3]) / 2.0;

  float scaleX = (right - left) / (float)(SCREEN_W - 2 * INSET);
  float scaleY = (bottom - top) / (float)(SCREEN_H - 2 * INSET);

  xMin = (int)(left   - INSET * scaleX);
  xMax = (int)(right  + INSET * scaleX);
  yMin = (int)(top    - INSET * scaleY);
  yMax = (int)(bottom + INSET * scaleY);
}

void report() {
  Serial.println("\n=========== CALIBRATION RESULT ===========");
  Serial.printf("#define RAW_X_MIN %d\n", xMin);
  Serial.printf("#define RAW_X_MAX %d\n", xMax);
  Serial.printf("#define RAW_Y_MIN %d\n", yMin);
  Serial.printf("#define RAW_Y_MAX %d\n", yMax);
  Serial.println("==========================================");
  if (xMin > xMax) Serial.println("NOTE: X axis inverted (map() handles it).");
  if (yMin > yMax) Serial.println("NOTE: Y axis inverted (map() handles it).");

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Calibration done");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 50);  tft.printf("RAW_X_MIN %d", xMin);
  tft.setCursor(10, 65);  tft.printf("RAW_X_MAX %d", xMax);
  tft.setCursor(10, 80);  tft.printf("RAW_Y_MIN %d", yMin);
  tft.setCursor(10, 95);  tft.printf("RAW_Y_MAX %d", yMax);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 130);
  tft.print("Touch to start verify mode");

  delay(500);
  waitRelease();
  while (!touchscreen.touched()) delay(10);
  waitRelease();
}

void verify() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print("VERIFY: dots should land under your finger");
  crosshair(INSET, INSET, TFT_DARKGREY);
  crosshair(SCREEN_W - INSET, INSET, TFT_DARKGREY);
  crosshair(SCREEN_W - INSET, SCREEN_H - INSET, TFT_DARKGREY);
  crosshair(INSET, SCREEN_H - INSET, TFT_DARKGREY);

  while (true) {
    if (touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      int sx = constrain(map(p.x, xMin, xMax, 0, SCREEN_W), 0, SCREEN_W - 1);
      int sy = constrain(map(p.y, yMin, yMax, 0, SCREEN_H), 0, SCREEN_H - 1);
      tft.fillCircle(sx, sy, 3, TFT_RED);
      tft.fillRect(0, 225, 320, 15, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(5, 228);
      tft.printf("raw %d,%d -> screen %d,%d", p.x, p.y, sx, sy);
      Serial.printf("[VERIFY] raw %d,%d -> screen %d,%d\n", p.x, p.y, sx, sy);
      delay(15);
    }
  }
}

}  // namespace calib

// Never returns
inline void runTouchCalibration() {
  Serial.println("\n--- CYD touch calibration ---");
  for (int i = 0; i < 4; i++) {
    while (!calib::capture(i)) {}
  }
  calib::compute();
  calib::report();
  calib::verify();
}

#endif  // RUN_CALIBRATION