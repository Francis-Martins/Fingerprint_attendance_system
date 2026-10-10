#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_Fingerprint.h>
#include <Adafruit_PN532.h>
#include <Firebase_ESP_Client.h>
#include "buzzer.h" 
#include "config.h"
#include "debug_trace.h"
#include "display_ui.h"
#include "touch_input.h"
#include "wifi_time.h"
#include "firebase_manager.h"
#include "fingerprint_module.h"
#include "rfid_module.h"
#include "app_state.h"

// ==================================================================
//  Setup
// ==================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  buzzerBegin();
  tft.init();
  tft.invertDisplay(true);
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  setState("WiFi: connecting");
  drawBanner("Connecting WiFi...", TFT_YELLOW);
  connectWiFi();

  setState("NTP: syncing time");
  drawBanner("Syncing time...", TFT_YELLOW);
  setupTime();

  setState("Firebase: connecting/auth");
  drawBanner("Connecting Firebase...", TFT_YELLOW);
  setupFirebase();



  setState("FP: init serial");
  fpSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX);
  delay(100);

  if (finger.verifyPassword()) {
    drawBanner("FP Sensor OK", TFT_GREEN);
  } else {
    drawBanner("FP Sensor NOT found!", TFT_RED);
    setState("HALTED: FP sensor not found");
    while (1) delay(1000);
  }

  setState("RFID: init");
  Wire.begin(RFID_SDA_PIN, RFID_SCL_PIN);
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (versiondata) {
    nfc.SAMConfig();
    rfidReady = true;
    drawBanner("RFID OK", TFT_GREEN);
  } else {
    rfidReady = false;
    drawBanner("RFID NOT found (continuing)", TFT_RED);
  }
  delay(600);

  setState("Touch: init");
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);

  delay(600);
  goHome();
}

// ==================================================================
//  Loop
// ==================================================================
void loop() {
  buzzerUpdate();
  setState("Listening for touch");
  handleTouch();

  switch (appMode) {
    case HOME:
      break;

    case MATCH_MODE:
      if (matchSubMode == FP_MODE) {
        pollFingerprint();
      } else {
        pollRFID();
      }
      break;

    case ADMIN_MODE:
      pollCommands();
      break;
  }
}