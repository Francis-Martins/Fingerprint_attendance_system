#ifndef RFID_MODULE_H
#define RFID_MODULE_H

#include <Arduino.h>
#include <Adafruit_PN532.h>

// ==================================================================
//  Shared RFID reader object
// ==================================================================
// Defined in rfid_module.cpp. main.cpp needs rfidReady and nfc
// directly for the setup()-time Wire.begin()/getFirmwareVersion()
// check, so both are exposed here rather than kept private.
extern Adafruit_PN532 nfc;
extern bool rfidReady;

// ==================================================================
//  Passive matching — polled every loop() while in MATCH_MODE / RFID_MODE
// ==================================================================
void pollRFID();

// ==================================================================
//  Admin command (triggered via firebase_manager::pollCommands)
// ==================================================================
void runEnrollRFID();

#endif // RFID_MODULE_H