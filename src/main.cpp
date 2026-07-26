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
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "secrets.h"

// ==================================================================
//  Display
// ==================================================================
TFT_eSPI tft = TFT_eSPI();
#define TFT_BACKLIGHT_PIN 21

// ==================================================================
//  Touch (XPT2046)
// ==================================================================
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define RAW_X_MIN 330
#define RAW_X_MAX 3720
#define RAW_Y_MIN 440
#define RAW_Y_MAX 3750

unsigned long lastTouchMs = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 250;

struct Btn {
  int x, y, w, h;
  const char* label;
};

Btn homeButtons[2] = {
  {40,  80, 100, 70, "MATCH"},
  {180, 80, 100, 70, "ADMIN"}
};

Btn menuButton = {260, 5, 55, 25, "MENU"};

// Toggle button shown only in MATCH_MODE idle screen
Btn toggleButton = {195, 5, 60, 25, "MODE"};

// ==================================================================
//  Fingerprint
// ==================================================================
#define FP_RX 22
#define FP_TX 27
HardwareSerial fpSerial(2);
Adafruit_Fingerprint finger(&fpSerial);

// ==================================================================
//  RFID (PN532, I2C)
//  NOTE: test wiring below. Final wiring will move to GPIO1/GPIO3 (UART0),
//  which requires Serial debugging disabled (already the plan since
//  device will run off external power, not USB).
// ==================================================================
#define RFID_SDA_PIN 4   
#define RFID_SCL_PIN 17  
Adafruit_PN532 nfc(-1, -1);
bool rfidReady = false;

// ==================================================================
//  Firebase
// ==================================================================
FirebaseData fbdo;
FirebaseData fbdoCmd;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseReady = false;

// ==================================================================
//  NTP
// ==================================================================
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 3600;
const int DAYLIGHT_OFFSET_SEC = 0;
bool timeReady = false;

// ==================================================================
//  App-level mode
// ==================================================================
enum AppMode { HOME, MATCH_MODE, ADMIN_MODE };
AppMode appMode = HOME;

enum MatchSubMode { FP_MODE, RFID_MODE };
MatchSubMode matchSubMode = FP_MODE;

enum ScanState { IDLE, CAPTURING, RESULT, COOLDOWN };
ScanState state = IDLE;

unsigned long resultStart = 0;
const unsigned long RESULT_DISPLAY_MS = 2000;

unsigned long cooldownStart = 0;
const unsigned long COOLDOWN_MS = 3000;

unsigned long lastCommandCheck = 0;
const unsigned long COMMAND_POLL_MS = 2500;

bool commandActive = false;
unsigned long commandDoneAt = 0;
const unsigned long COMMAND_RESULT_HOLD_MS = 3000;

// ---------- Forward decls ----------
void connectWiFi();
void setupFirebase();
void setupTime();
String getDateString();
String getTimeString();
String getUserNameByFingerprintID(int fingerID);
String getUserNameByRFID(String uid, String &matchedKey);
void logAttendance(String name, String mode, String idField, String idValue);

void pollFingerprint();
void pollRFID();
void pollCommands();
void finishCommand(bool ok, const char* message, int assignedID = -1, String assignedUID = "");
void runEnroll();
void runEnrollRFID();
void runDelete(int targetID);
int getFreeID();

void handleTouch();
void goHome();
void goMatchMode();
void goAdminMode();

void drawHomeScreen();
void drawIdleScreen();
void drawCapturingScreen();
void drawResultScreen(bool success, String idLabel, int confidence, const char* note);
void drawBanner(String msg, uint16_t color);
void drawCommandScreen(String msg, uint16_t color);
void drawMenuButton();
void drawToggleButton();
void drawButton(Btn b);

// ==================================================================
//  Setup
// ==================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.invertDisplay(true);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  drawBanner("Connecting WiFi...", TFT_YELLOW);
  connectWiFi();

  drawBanner("Syncing time...", TFT_YELLOW);
  setupTime();

  drawBanner("Connecting Firebase...", TFT_YELLOW);
  setupFirebase();

  fpSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX);
  delay(100);

  if (finger.verifyPassword()) {
    drawBanner("FP Sensor OK", TFT_GREEN);
  } else {
    drawBanner("FP Sensor NOT found!", TFT_RED);
    while (1) delay(1000);
  }

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

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);

  delay(600);
  goHome();
}

// ==================================================================
//  Loop
// ==================================================================
void loop() {
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

// ==================================================================
//  Touch handling
// ==================================================================
void handleTouch() {
  if (!touchscreen.touched()) return;
  if (millis() - lastTouchMs < TOUCH_DEBOUNCE_MS) return;

  TS_Point p = touchscreen.getPoint();
  int screenX = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, 320);
  int screenY = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, 240);
  screenX = constrain(screenX, 0, 320);
  screenY = constrain(screenY, 0, 240);

  lastTouchMs = millis();

  if (appMode == HOME) {
    for (int i = 0; i < 2; i++) {
      Btn b = homeButtons[i];
      if (screenX >= b.x && screenX <= (b.x + b.w) &&
          screenY >= b.y && screenY <= (b.y + b.h)) {
        if (String(b.label) == "MATCH") {
          goMatchMode();
        } else if (String(b.label) == "ADMIN") {
          goAdminMode();
        }
        return;
      }
    }
    return;
  }

  if (appMode == ADMIN_MODE && commandActive) return;

  // MENU button (both MATCH_MODE and ADMIN_MODE)
  Btn b = menuButton;
  if (screenX >= b.x && screenX <= (b.x + b.w) &&
      screenY >= b.y && screenY <= (b.y + b.h)) {
    goHome();
    return;
  }

  // TOGGLE button — only live in MATCH_MODE, and only when idle
  // (don't let a toggle interrupt an active capture)
  if (appMode == MATCH_MODE && state == IDLE) {
    Btn t = toggleButton;
    if (screenX >= t.x && screenX <= (t.x + t.w) &&
        screenY >= t.y && screenY <= (t.y + t.h)) {
      matchSubMode = (matchSubMode == FP_MODE) ? RFID_MODE : FP_MODE;
      drawIdleScreen();
    }
  }
}

// ==================================================================
//  Mode transitions
// ==================================================================
void goHome() {
  appMode = HOME;
  state = IDLE;
  commandActive = false;
  drawHomeScreen();
}

void goMatchMode() {
  appMode = MATCH_MODE;
  state = IDLE;
  drawIdleScreen();
}

void goAdminMode() {
  appMode = ADMIN_MODE;
  commandActive = false;
  lastCommandCheck = 0;
  drawCommandScreen("Admin mode.\nWaiting for command...", TFT_CYAN);
}

// ==================================================================
//  Command polling (enroll fingerprint / enroll RFID / delete)
// ==================================================================
void pollCommands() {
  if (commandActive) {
    if (millis() - commandDoneAt >= COMMAND_RESULT_HOLD_MS) {
      commandActive = false;
      drawCommandScreen("Admin mode.\nWaiting for command...", TFT_CYAN);
    }
    return;
  }

  if (millis() - lastCommandCheck < COMMAND_POLL_MS) return;
  lastCommandCheck = millis();

  if (!firebaseReady || !Firebase.ready()) return;

  if (!Firebase.RTDB.getString(&fbdoCmd, "/commands/action")) {
    return;
  }

  String action = fbdoCmd.stringData();

  if (action == "enroll") {
    Firebase.RTDB.setString(&fbdoCmd, "/commands/status", "in_progress");
    commandActive = true;
    runEnroll();
    commandDoneAt = millis();

  } else if (action == "enroll_rfid") {
    Firebase.RTDB.setString(&fbdoCmd, "/commands/status", "in_progress");
    commandActive = true;
    runEnrollRFID();
    commandDoneAt = millis();

  } else if (action == "delete") {
    commandActive = true;
    if (Firebase.RTDB.getInt(&fbdoCmd, "/commands/targetID")) {
      int targetID = fbdoCmd.intData();
      Firebase.RTDB.setString(&fbdoCmd, "/commands/status", "in_progress");
      runDelete(targetID);
    } else {
      finishCommand(false, "No targetID provided");
    }
    commandDoneAt = millis();
  }
}

void finishCommand(bool ok, const char* message, int assignedID, String assignedUID) {
  if (assignedID >= 0) {
    Firebase.RTDB.setInt(&fbdoCmd, "/commands/assignedID", assignedID);
  }
  if (assignedUID.length() > 0) {
    Firebase.RTDB.setString(&fbdoCmd, "/commands/assignedUID", assignedUID);
  }
  Firebase.RTDB.setString(&fbdoCmd, "/commands/message", message);
  Firebase.RTDB.setString(&fbdoCmd, "/commands/status", ok ? "success" : "failed");
  Firebase.RTDB.setString(&fbdoCmd, "/commands/action", "none");
}

// ==================================================================
//  Enroll (fingerprint) — unchanged logic
// ==================================================================
int getFreeID() {
  for (int id = 1; id < 127; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) return id;
  }
  return -1;
}

void runEnroll() {
  int id = getFreeID();
  if (id == -1) {
    drawCommandScreen("No free slots.", TFT_RED);
    finishCommand(false, "No free slots");
    return;
  }

  int p = -1;
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

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    drawCommandScreen("Convert failed.", TFT_RED);
    finishCommand(false, "Convert failed");
    return;
  }

  drawCommandScreen("Remove finger.", TFT_YELLOW);
  delay(1500);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) p = finger.getImage();

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

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    drawCommandScreen("Convert failed.", TFT_RED);
    finishCommand(false, "Convert failed (2nd)");
    return;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    drawCommandScreen("Prints didn't\nmatch. Retry.", TFT_RED);
    finishCommand(false, "Prints did not match");
    return;
  }

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
//  Enroll (RFID) — scan once, return UID
// ==================================================================
void runEnrollRFID() {
  if (!rfidReady) {
    drawCommandScreen("RFID not ready.", TFT_RED);
    finishCommand(false, "RFID reader not initialized");
    return;
  }

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

void runDelete(int targetID) {
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
      int p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        state = CAPTURING;
        drawCapturingScreen();
      }
      break;
    }

    case CAPTURING: {
      int p = finger.image2Tz();
      if (p != FINGERPRINT_OK) {
        drawResultScreen(false, "", 0, "Scan error, try again");
        resultStart = millis();
        state = RESULT;
        break;
      }

      p = finger.fingerFastSearch();
      if (p == FINGERPRINT_OK) {
        if (finger.confidence >= 50) {
          String name = getUserNameByFingerprintID(finger.fingerID);
          logAttendance(name, "fingerprint", "Fingerprint ID", String(finger.fingerID));
          drawResultScreen(true, "ID #" + String(finger.fingerID), finger.confidence, name.c_str());
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

// ==================================================================
//  Passive RFID matching — MATCH_MODE / RFID_MODE
// ==================================================================
void pollRFID() {
  switch (state) {

    case IDLE: {
      if (!rfidReady) return;
      uint8_t uidBytes[7];
      uint8_t uidLength;
      bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBytes, &uidLength, 300);
      if (found) {
        String uidStr = "";
        for (int i = 0; i < uidLength; i++) {
          if (uidBytes[i] < 0x10) uidStr += "0";
          uidStr += String(uidBytes[i], HEX);
        }
        uidStr.toUpperCase();

        String matchedKey = "";
        String name = getUserNameByRFID(uidStr, matchedKey);

        if (matchedKey.length() > 0) {
          logAttendance(name, "rfid", "RFID UID", uidStr);
          drawResultScreen(true, uidStr, 0, name.c_str());
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

// ==================================================================
//  Screens
// ==================================================================
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

// ==================================================================
//  WiFi
// ==================================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.print("\nConnected. IP: ");
  Serial.println(WiFi.localIP());

  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
}

// ==================================================================
//  Time (NTP)
// ==================================================================
void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  unsigned long start = millis();
  while (!getLocalTime(&timeinfo) && millis() - start < 10000) {
    delay(200);
  }

  timeReady = getLocalTime(&timeinfo);
  if (timeReady) {
    Serial.println("Time synced.");
  } else {
    Serial.println("Time sync failed, Date/Time fields will be inaccurate.");
  }
}

String getDateString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "unknown";
  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
  return String(buf);
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "unknown";
  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  return String(buf);
}

// ==================================================================
//  Firebase
// ==================================================================
void setupFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  unsigned long start = millis();
  while (auth.token.uid.length() == 0 && millis() - start < 15000) {
    delay(200);
  }

  firebaseReady = (auth.token.uid.length() > 0);
}

// ==================================================================
//  User lookups
//  NOTE: /users is now keyed by push ID, not fingerprint ID.
//  Each user node may have fingerprintID and/or rfidUID fields.
// ==================================================================
String getUserNameByFingerprintID(int fingerID) {
  if (!firebaseReady || !Firebase.ready()) {
    return "Unknown (ID #" + String(fingerID) + ")";
  }

  if (Firebase.RTDB.getJSON(&fbdo, "/users")) {
    FirebaseJson *json = fbdo.jsonObjectPtr();
    FirebaseJsonData result;
    size_t count = json->iteratorBegin();
    String key, value;
    int type;
    for (size_t i = 0; i < count; i++) {
      json->iteratorGet(i, type, key, value);
      // top-level keys only
      if (type == FirebaseJson::JSON_OBJECT && key.indexOf('/') == -1) {
        FirebaseJson userObj;
        userObj.setJsonData(value);
        FirebaseJsonData fpData, nameData;
        if (userObj.get(fpData, "fingerprintID") && fpData.intValue == fingerID) {
          if (userObj.get(nameData, "name")) {
            json->iteratorEnd();
            return nameData.stringValue;
          }
        }
      }
    }
    json->iteratorEnd();
  }

  return "Unknown (ID #" + String(fingerID) + ")";
}

String getUserNameByRFID(String uid, String &matchedKey) {
  if (!firebaseReady || !Firebase.ready()) {
    return "Unknown (UID " + uid + ")";
  }

  if (Firebase.RTDB.getJSON(&fbdo, "/users")) {
    FirebaseJson *json = fbdo.jsonObjectPtr();
    size_t count = json->iteratorBegin();
    String key, value;
    int type;
    for (size_t i = 0; i < count; i++) {
      json->iteratorGet(i, type, key, value);
      if (type == FirebaseJson::JSON_OBJECT && key.indexOf('/') == -1) {
        FirebaseJson userObj;
        userObj.setJsonData(value);
        FirebaseJsonData uidData, nameData;
        if (userObj.get(uidData, "rfidUID") && uidData.stringValue == uid) {
          matchedKey = key;
          if (userObj.get(nameData, "name")) {
            json->iteratorEnd();
            return nameData.stringValue;
          }
        }
      }
    }
    json->iteratorEnd();
  }

  matchedKey = "";
  return "Unknown (UID " + uid + ")";
}

void logAttendance(String name, String mode, String idField, String idValue) {
  if (!firebaseReady || !Firebase.ready()) {
    Serial.println("Firebase not ready, skipping log.");
    return;
  }

  FirebaseJson json;
  json.set("mode", mode);
  json.set(idField, idValue);
  json.set("Name", name);
  json.set("Status", "Present");
  json.set("Date", getDateString());
  json.set("Time", getTimeString());
  json.set("timestamp/.sv", "timestamp");

  String path = "/attendance";

  if (Firebase.RTDB.pushJSON(&fbdo, path.c_str(), &json)) {
    Serial.println("Attendance logged to Firebase.");
  } else {
    Serial.print("Firebase push failed: ");
    Serial.println(fbdo.errorReason());
  }
}