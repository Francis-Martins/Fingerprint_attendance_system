#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>

// ==================================================================
//  App-level mode / state enums
// ==================================================================
enum AppMode { HOME, MATCH_MODE, ADMIN_MODE };
enum MatchSubMode { FP_MODE, RFID_MODE };
enum ScanState { IDLE, CAPTURING, RESULT, COOLDOWN };

// ==================================================================
//  Timing constants tied to state transitions
// ==================================================================
extern const unsigned long RESULT_DISPLAY_MS;      // how long RESULT screen stays up
extern const unsigned long COOLDOWN_MS;             // pause after result before re-arming

// ==================================================================
//  Shared state globals (defined in app_state.cpp)
// ==================================================================
extern AppMode appMode;
extern MatchSubMode matchSubMode;
extern ScanState state;

extern unsigned long resultStart;
extern unsigned long cooldownStart;

extern bool commandActive;
extern unsigned long commandDoneAt;

// ==================================================================
//  Mode transition functions
// ==================================================================
void goHome();
void goMatchMode();
void goAdminMode();

#endif // APP_STATE_H