#include "touch_input.h"
#include "app_state.h"
#include "display_ui.h"
#include "debug_trace.h"
#include "config.h"

// ==================================================================
//  Shared touchscreen objects
// ==================================================================
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ==================================================================
//  Debounce — module-local timing
// ==================================================================
static unsigned long lastTouchMs = 0;

// ==================================================================
//  Touch polling
// ==================================================================
void handleTouch() {
  if (!touchscreen.touched()) return;

  TS_Point p = touchscreen.getPoint();

  // Print raw coordinates immediately, before any debounce logic,
  // so we always see exactly what the touch controller reported.
  Serial.print("[RAW] x=");
  Serial.print(p.x);
  Serial.print(" y=");
  Serial.println(p.y);

  unsigned long nowMs = millis();
  if (nowMs - lastTouchMs < TOUCH_DEBOUNCE_MS) {
    return;  // still print raw above, just skip further action
  }

  int screenX = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, 320);
  int screenY = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, 240);
  screenX = constrain(screenX, 0, 320);
  screenY = constrain(screenY, 0, 240);
  drawTouchMarker(screenX, screenY);
  lastTouchMs = millis();

  Serial.print("[TOUCH] Raw x=");
  Serial.print(p.x);
  Serial.print(" y=");
  Serial.print(p.y);
  Serial.print("  ->  Screen x=");
  Serial.print(screenX);
  Serial.print(" y=");
  Serial.print(screenY);
  Serial.print("  at millis=");
  Serial.print(nowMs);
  Serial.print("  [STATE at tap time: ");
  Serial.print(currentState);
  Serial.println("]");

  if (appMode == HOME) {
    Serial.println("[TOUCH] appMode = HOME");
    for (int i = 0; i < 2; i++) {
      Btn b = homeButtons[i];
      if (screenX >= b.x && screenX <= (b.x + b.w) &&
          screenY >= b.y && screenY <= (b.y + b.h)) {
        Serial.print("[TOUCH] Hit button: ");
        Serial.println(b.label);
        if (String(b.label) == "MATCH") {
          Serial.println("[TOUCH] -> goMatchMode()");
          goMatchMode();
        } else if (String(b.label) == "ADMIN") {
          Serial.println("[TOUCH] -> goAdminMode()");
          goAdminMode();
        }
        return;
      }
    }
    Serial.println("[TOUCH] No button hit on HOME screen");
    return;
  }

  if (appMode == ADMIN_MODE && commandActive) {
    Serial.println("[TOUCH] Ignored - ADMIN_MODE command active");
    return;
  }

  // MENU button
  Btn b = menuButton;
  if (screenX >= b.x && screenX <= (b.x + b.w) &&
      screenY >= b.y && screenY <= (b.y + b.h)) {
    Serial.println("[TOUCH] Hit MENU button -> goHome()");
    goHome();
    return;
  }

  // TOGGLE button
  if (appMode == MATCH_MODE && state == IDLE) {
    Btn t = toggleButton;
    if (screenX >= t.x && screenX <= (t.x + t.w) &&
        screenY >= t.y && screenY <= (t.y + t.h)) {
      Serial.println("[TOUCH] Hit TOGGLE button");
      matchSubMode = (matchSubMode == FP_MODE) ? RFID_MODE : FP_MODE;
      drawIdleScreen();
      return;
    }
  }

  Serial.println("[TOUCH] Touch registered but no button matched");
}