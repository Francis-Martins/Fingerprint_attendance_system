#include "rfid_module.h"
#include "app_state.h"
#include "display_ui.h"
#include "firebase_manager.h"
#include "debug_trace.h"
#include "config.h"
#include "buzzer.h" 

// ==================================================================
//  Shared RFID reader object
// ==================================================================
Adafruit_PN532 nfc(-1, -1);
bool rfidReady = false;

// ==================================================================
//  Passive polling — module-local timing
// ==================================================================
static unsigned long lastRFIDPoll = 0;

// ==================================================================
//  Admin: enroll RFID tag — scan once, return UID
// ==================================================================
void runEnrollRFID() {
  if (!rfidReady) {
    drawCommandScreen("RFID not ready.", TFT_RED);
    finishCommand(false, "RFID reader not initialized");
    return;
  }

  setState("RFID Enroll: waiting for tag");
  drawCommandScreen("Tap card/tag...", TFT_CYAN);

  uint8_t uidBytes[7];
  uint8_t uidLength;
  unsigned long stepStart = millis();
  bool found = false;

  while (millis() - stepStart < 15000) {
    found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBytes, &uidLength, 500);
    if (found) break;
  }

  if (!found) {
    drawCommandScreen("Timed out.", TFT_RED);
    finishCommand(false, "Timed out waiting for tag");
    return;
  }

  String uidStr = "";
  for (int i = 0; i < uidLength; i++) {
    if (uidBytes[i] < 0x10) uidStr += "0";
    uidStr += String(uidBytes[i], HEX);
  }
  uidStr.toUpperCase();

  drawCommandScreen("Tag read:\n" + uidStr, TFT_GREEN);
  finishCommand(true, "RFID scanned successfully", -1, uidStr);
}

// ==================================================================
//  Passive RFID matching — MATCH_MODE / RFID_MODE
// ==================================================================
void pollRFID() {
  switch (state) {

    case IDLE: {
      if (!rfidReady) return;

      if (millis() - lastRFIDPoll < RFID_POLL_INTERVAL_MS) return;
      lastRFIDPoll = millis();

      setState("RFID: readPassiveTargetID");
      uint8_t uidBytes[7];
      uint8_t uidLength;

      bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBytes, &uidLength, RFID_READ_TIMEOUT_MS);

      if (found) {
        String uidStr = "";
        for (int i = 0; i < uidLength; i++) {
          if (uidBytes[i] < 0x10) uidStr += "0";
          uidStr += String(uidBytes[i], HEX);
        }
        uidStr.toUpperCase();

        String matchedKey = "";
        String regNo;
        String name = getUserNameByRFID(uidStr, matchedKey, regNo);

        if (matchedKey.length() > 0) {
          logAttendance(name, regNo, "rfid", "RFID UID", uidStr);
          drawResultScreen(true, uidStr, 0, name.c_str());
          buzzerStart(); 
        } else {
          drawResultScreen(false, uidStr, 0, "Not Recognized");
        }

        resultStart = millis();
        state = RESULT;
      }

      break;
    }

    case RESULT: {
      if (millis() - resultStart >= RESULT_DISPLAY_MS) {
        cooldownStart = millis();
        state = COOLDOWN;
      }
      break;
    }

    case COOLDOWN: {
      if (millis() - cooldownStart >= COOLDOWN_MS) {
        state = IDLE;
        drawIdleScreen();
      }
      break;
    }

    default:
      state = IDLE;
      break;
  }
}