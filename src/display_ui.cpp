#include "display_ui.h"
#include "app_state.h"

// ==================================================================
//  Shared display object
// ==================================================================
TFT_eSPI tft = TFT_eSPI();

// ==================================================================
//  UI primitives
// ==================================================================
void drawTouchMarker(int x, int y) {
  tft.fillCircle(x, y, 4, TFT_RED);
}

void drawButton(Btn b) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, TFT_DARKGREY);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setCursor(b.x + 10, b.y + b.h / 2 - 4);
  tft.print(b.label);
}

void drawMenuButton() {
  drawButton(menuButton);
}

void drawToggleButton() {
  Btn t = toggleButton;
  t.label = (matchSubMode == FP_MODE) ? "FP" : "RFID";
  drawButton(t);
}

// ==================================================================
//  Screens
// ==================================================================
void drawHomeScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(70, 30);
  tft.println("Select Mode");

  tft.setTextSize(1);
  drawButton(homeButtons[0]);
  drawButton(homeButtons[1]);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(10, 220);
  tft.print("System ready");
}

void drawIdleScreen() {
  tft.fillScreen(TFT_BLACK);
  drawMenuButton();
  drawToggleButton();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 90);
  if (matchSubMode == FP_MODE) {
    tft.println("Place finger to");
    tft.setCursor(20, 115);
    tft.println("check in");
  } else {
    tft.println("Tap card/tag");
    tft.setCursor(20, 115);
    tft.println("to check in");
  }

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(10, 220);
  tft.print(matchSubMode == FP_MODE ? "Fingerprint mode" : "RFID mode");
}

void drawCapturingScreen() {
  tft.fillScreen(TFT_BLACK);
  drawMenuButton();

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(60, 100);
  tft.println("Scanning...");
}

void drawResultScreen(bool success, String idLabel, int confidence, const char* note) {
  tft.fillScreen(TFT_BLACK);
  drawMenuButton();

  uint16_t color = success ? TFT_GREEN : TFT_RED;
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(3);

  const char* symbol = success ? "OK" : "X";
  tft.setCursor(140, 50);
  tft.println(symbol);

  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println(note);

  if (idLabel.length() > 0) {
    tft.setTextSize(1);
    tft.setCursor(20, 140);
    tft.print(idLabel);
    if (confidence > 0) {
      tft.print("   Conf: ");
      tft.println(confidence);
    }
  }
}

void drawBanner(String msg, uint16_t color) {
  Serial.println(msg);
  tft.fillRect(0, 0, 320, 30, TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print(msg);
}

void drawCommandScreen(String msg, uint16_t color) {
  Serial.println(msg);
  tft.fillScreen(TFT_BLACK);
  if (!commandActive) drawMenuButton();

  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 100);
  tft.println(msg);
}