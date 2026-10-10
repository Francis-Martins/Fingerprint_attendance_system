#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#include <Arduino.h>

// ==================================================================
//  STATE TRACE (debug instrumentation)
// ==================================================================
// Tracks the current high-level operation the firmware is in and
// prints a transition line (with how long the previous state lasted)
// whenever it changes. Called from every other module to make the
// serial log read like a timeline instead of scattered print()s.
// ==================================================================

extern const char* currentState;
extern unsigned long lastStateChangeMs;

// Call this whenever the firmware enters a new logical state.
// Only prints if newState differs from currentState (via strcmp),
// so it's safe to call every loop() tick without spamming serial.
void setState(const char* newState);

#endif // DEBUG_TRACE_H