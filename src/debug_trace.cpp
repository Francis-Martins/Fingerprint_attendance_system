#include "debug_trace.h"

// ==================================================================
//  STATE TRACE (debug instrumentation)
// ==================================================================
const char* currentState = "Booting";
unsigned long lastStateChangeMs = 0;

void setState(const char* newState) {
  unsigned long now = millis();
  // strcmp so we only print on genuine change, not every loop tick
  if (strcmp(currentState, newState) != 0) {
    unsigned long duration = now - lastStateChangeMs;
    Serial.print("[STATE] ");
    Serial.print(currentState);
    Serial.print("  (held for ");
    Serial.print(duration);
    Serial.print(" ms)  ->  ");
    Serial.println(newState);
    currentState = newState;
    lastStateChangeMs = now;
  }
}