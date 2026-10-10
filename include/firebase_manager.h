#ifndef FIREBASE_MANAGER_H
#define FIREBASE_MANAGER_H

#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include "config.h"

// ==================================================================
//  Shared Firebase objects
// ==================================================================
extern FirebaseData fbdo;      // used for logging + user lookups
extern FirebaseData fbdoCmd;   // used for admin command polling
extern FirebaseAuth auth;
extern FirebaseConfig config;
extern bool firebaseReady;

// ==================================================================
//  Local user cache — populated once at boot, re-used for lookups
// ==================================================================


// ==================================================================
//  Setup / cache
// ==================================================================
void setupFirebase();

// ==================================================================
//  Lookups
// ==================================================================
String getUserNameByFingerprintID(int fingerID, String &regNo);
String getUserNameByRFID(String uid, String &matchedKey, String &regNo);

// ==================================================================
//  Logging
// ==================================================================
void logAttendance(String name, String regNo, String mode, String idField, String idValue);

// ==================================================================
//  Admin command polling (enroll fingerprint / enroll RFID / delete)
// ==================================================================
extern const unsigned long COMMAND_RESULT_HOLD_MS;

void pollCommands();
void finishCommand(bool ok, const char* message, int assignedID = -1, String assignedUID = "");

#endif // FIREBASE_MANAGER_H