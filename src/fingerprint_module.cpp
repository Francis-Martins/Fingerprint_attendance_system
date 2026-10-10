#include "fingerprint_module.h"
#include "app_state.h"
#include "display_ui.h"
#include "firebase_manager.h"
#include "debug_trace.h"
#include "config.h"
#include "buzzer.h"   

// ==================================================================
//  Shared fingerprint sensor object
// ==================================================================
HardwareSerial fpSerial(2);
Adafruit_Fingerprint finger(&fpSerial);

// ==================================================================
//  Admin: find first free enrollment slot
// ==================================================================
int getFreeID() {
  setState("FP: getFreeID() scanning slots");
  for (int id = 1; id < 127; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) return id;
  }
  return -1;
}

// ==================================================================
//  Admin: enroll new fingerprint
// ==================================================================
void runEnroll() {
  int id = getFreeID();
  if (id == -1) {
    drawCommandScreen("No free slots.", TFT_RED);
    finishCommand(false, "No free slots");
    return;
  }

  int p = -1;
  setState("FP Enroll: waiting for finger (1st)");
  drawCommandScreen("Place finger...", TFT_CYAN);
  unsigned long stepStart = millis();
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p != FINGERPRINT_OK && p != FINGERPRINT_NOFINGER) {
      drawCommandScreen("Image error.", TFT_RED);
      finishCommand(false, "Image error");
      return;
    }
    if (millis() - stepStart > 15000) {
      drawCommandScreen("Timed out.", TFT_RED);
      finishCommand(false, "Timed out waiting for finger");
      return;
    }
  }

  setState("FP Enroll: image2Tz (1st)");
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    drawCommandScreen("Convert failed.", TFT_RED);
    finishCommand(false, "Convert failed");
    return;
  }

  setState("FP Enroll: remove finger delay");
  drawCommandScreen("Remove finger.", TFT_YELLOW);
  delay(1500);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) p = finger.getImage();

  setState("FP Enroll: waiting for finger (2nd)");
  drawCommandScreen("Place same\nfinger again...", TFT_CYAN);
  p = -1;
  stepStart = millis();
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p != FINGERPRINT_OK && p != FINGERPRINT_NOFINGER) {
      drawCommandScreen("Image error.", TFT_RED);
      finishCommand(false, "Image error (2nd)");
      return;
    }
    if (millis() - stepStart > 15000) {
      drawCommandScreen("Timed out.", TFT_RED);
      finishCommand(false, "Timed out (2nd placement)");
      return;
    }
  }

  setState("FP Enroll: image2Tz (2nd)");
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    drawCommandScreen("Convert failed.", TFT_RED);
    finishCommand(false, "Convert failed (2nd)");
    return;
  }

  setState("FP Enroll: createModel");
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    drawCommandScreen("Prints didn't\nmatch. Retry.", TFT_RED);
    finishCommand(false, "Prints did not match");
    return;
  }

  setState("FP Enroll: storeModel");
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    drawCommandScreen("Enrolled! ID #" + String(id), TFT_GREEN);
    finishCommand(true, "Enrolled successfully", id);
  } else {
    drawCommandScreen("Store failed.", TFT_RED);
    finishCommand(false, "Store failed");
  }
}

// ==================================================================
//  Admin: delete a fingerprint by ID
// ==================================================================
void runDelete(int targetID) {
  setState("FP: deleteModel");
  drawCommandScreen("Deleting ID #" + String(targetID) + "...", TFT_YELLOW);

  int p = finger.deleteModel(targetID);
  if (p == FINGERPRINT_OK) {
    drawCommandScreen("Deleted ID #" + String(targetID), TFT_GREEN);
    finishCommand(true, "Deleted successfully");
  } else {
    drawCommandScreen("Delete failed.", TFT_RED);
    finishCommand(false, "Delete failed");
  }
}

// ==================================================================
//  Passive fingerprint matching — MATCH_MODE / FP_MODE
// ==================================================================
void pollFingerprint() {
  switch (state) {

    case IDLE: {
      setState("FP: getImage (idle scan)");
      int p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        state = CAPTURING;
        drawCapturingScreen();
      }
      break;
    }

    case CAPTURING: {
      setState("FP: image2Tz (match)");
      int p = finger.image2Tz();
      if (p != FINGERPRINT_OK) {
        drawResultScreen(false, "", 0, "Scan error, try again");
        resultStart = millis();
        state = RESULT;
        break;
      }

      setState("FP: fingerFastSearch");
      p = finger.fingerFastSearch();
      if (p == FINGERPRINT_OK) {
        if (finger.confidence >= 50) {
          String regNo;
          String name = getUserNameByFingerprintID(finger.fingerID, regNo);
          if (name.startsWith("Unknown")) {
            drawResultScreen(false, "ID #" + String(finger.fingerID), finger.confidence, "User not found");
          } else {
            logAttendance(name, regNo, "fingerprint", "Fingerprint ID", String(finger.fingerID));
            drawResultScreen(true, "ID #" + String(finger.fingerID), finger.confidence, name.c_str());
            buzzerStart();
}
        } else {
          drawResultScreen(false, "ID #" + String(finger.fingerID), finger.confidence, "Confidence too low");
        }
      } else if (p == FINGERPRINT_NOTFOUND) {
        drawResultScreen(false, "", 0, "Not Recognized");
      } else {
        drawResultScreen(false, "", 0, "Match error");
      }

      resultStart = millis();
      state = RESULT;
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
  }
}