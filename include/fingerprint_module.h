#ifndef FINGERPRINT_MODULE_H
#define FINGERPRINT_MODULE_H

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>

// ==================================================================
//  Shared fingerprint sensor object
// ==================================================================
// Defined in fingerprint_module.cpp. main.cpp needs this directly
// for the setup()-time fpSerial.begin() / finger.verifyPassword()
// sensor check, so it's exposed here rather than kept private.
extern HardwareSerial fpSerial;
extern Adafruit_Fingerprint finger;

// ==================================================================
//  Passive matching — polled every loop() while in MATCH_MODE / FP_MODE
// ==================================================================
void pollFingerprint();

// ==================================================================
//  Admin commands (triggered via firebase_manager::pollCommands)
// ==================================================================
void runEnroll();
void runDelete(int targetID);
int getFreeID();

#endif // FINGERPRINT_MODULE_H