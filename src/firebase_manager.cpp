#include "firebase_manager.h"
#include "app_state.h"
#include "fingerprint_module.h"
#include "rfid_module.h"
#include "display_ui.h"
#include "wifi_time.h"
#include "debug_trace.h"
#include "secrets.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ==================================================================
//  Shared Firebase objects
// ==================================================================
FirebaseData fbdo;
FirebaseData fbdoCmd;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseReady = false;



// ==================================================================
//  Admin command polling — module-local timing
// ==================================================================
const unsigned long COMMAND_RESULT_HOLD_MS = 3000;

static unsigned long lastCommandCheck = 0;

// ==================================================================
//  Setup
// ==================================================================
void setupFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  fbdo.setBSSLBufferSize(4096, 1024);      // <-- add
  fbdoCmd.setBSSLBufferSize(4096, 1024);   // <-- add
  fbdo.setResponseSize(4096);   

  unsigned long start = millis();
  while (auth.token.uid.length() == 0 && millis() - start < 15000) {
    delay(200);
  }

  firebaseReady = (auth.token.uid.length() > 0);
}

// ==================================================================
//  Lookups — query Firebase for the single matching user, no cache
// ==================================================================

// Reads the first user object from the last query result in fbdo.
static bool readFirstUser(String &name, String &regNo) {
  FirebaseJson *json = fbdo.jsonObjectPtr();
  size_t count = json->iteratorBegin();
  String key, value;
  int type;
  bool found = false;

  for (size_t i = 0; i < count; i++) {
    json->iteratorGet(i, type, key, value);
    if (type != FirebaseJson::JSON_OBJECT) continue;

    FirebaseJson userObj;
    userObj.setJsonData(value);
    FirebaseJsonData nameData, regData;
    name  = userObj.get(nameData, "name")  ? nameData.stringValue : "Unknown";
    regNo = userObj.get(regData,  "regNo") ? regData.stringValue  : "N/A";
    found = true;
    break;
  }
  json->iteratorEnd();
  return found;
}

String getUserNameByFingerprintID(int fingerID, String &regNo) {
  regNo = "N/A";
  if (!firebaseReady || !Firebase.ready()) {
    return "Unknown (ID #" + String(fingerID) + ")";
  }

  setState("Firebase: query user by fingerprintID");
  QueryFilter q;
  q.orderBy("fingerprintID");
  q.equalTo(fingerID);

  if (Firebase.RTDB.getJSON(&fbdo, "/users", &q)) {
    String name;
    if (fbdo.dataType() == "json" && readFirstUser(name, regNo)) {
      return name;
    }
  } else {
    Serial.print("FP lookup failed: ");
    Serial.println(fbdo.errorReason());
  }
  regNo = "N/A";
  return "Unknown (ID #" + String(fingerID) + ")";
}

String getUserNameByRFID(String uid, String &matchedKey, String &regNo) {
  matchedKey = "";
  regNo = "N/A";
  if (!firebaseReady || !Firebase.ready()) {
    return "Unknown (UID " + uid + ")";
  }

  setState("Firebase: query user by rfidUID");
  QueryFilter q;
  q.orderBy("rfidUID");
  q.equalTo(uid.c_str());

  if (Firebase.RTDB.getJSON(&fbdo, "/users", &q)) {
    String name;
    if (fbdo.dataType() == "json" && readFirstUser(name, regNo)) {
      matchedKey = uid;
      return name;
    }
  } else {
    Serial.print("RFID lookup failed: ");
    Serial.println(fbdo.errorReason());
  }
  regNo = "N/A";
  return "Unknown (UID " + uid + ")";
}
// ==================================================================
//  Logging
// ==================================================================
void logAttendance(String name, String regNo, String mode, String idField, String idValue) {
  if (!firebaseReady || !Firebase.ready()) {
    Serial.println("Firebase not ready, skipping log.");
    return;
  }

  setState("Firebase: pushJSON (logAttendance)");
  FirebaseJson json;
  json.set("mode", mode);
  json.set(idField, idValue);
  json.set("Name", name);
  json.set("RegNo", regNo);
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
// ==================================================================
//  Admin command polling (enroll fingerprint / enroll RFID / delete)
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

  setState("Firebase: getString(commands/action)");
  if (!Firebase.RTDB.getString(&fbdoCmd, "/commands/action")) {
    return;
  }

  String action = fbdoCmd.stringData();

  if (action == "enroll") {
    setState("Firebase: setString(status=in_progress)");
    Firebase.RTDB.setString(&fbdoCmd, "/commands/status", "in_progress");
    commandActive = true;
    setState("Command: runEnroll()");
    runEnroll();
    commandDoneAt = millis();

  } else if (action == "enroll_rfid") {
    setState("Firebase: setString(status=in_progress)");
    Firebase.RTDB.setString(&fbdoCmd, "/commands/status", "in_progress");
    commandActive = true;
    setState("Command: runEnrollRFID()");
    runEnrollRFID();
    commandDoneAt = millis();

  } else if (action == "delete") {
    commandActive = true;
    setState("Firebase: getInt(commands/targetID)");
    if (Firebase.RTDB.getInt(&fbdoCmd, "/commands/targetID")) {
      int targetID = fbdoCmd.intData();
      setState("Firebase: setString(status=in_progress)");
      Firebase.RTDB.setString(&fbdoCmd, "/commands/status", "in_progress");
      setState("Command: runDelete()");
      runDelete(targetID);
    } else {
      finishCommand(false, "No targetID provided");
    }
    commandDoneAt = millis();
  }
}

void finishCommand(bool ok, const char* message, int assignedID, String assignedUID) {
  setState("Firebase: finishCommand() writes");
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