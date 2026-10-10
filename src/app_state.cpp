#include "app_state.h"
#include "display_ui.h"

// ==================================================================
//  Timing constants
// ==================================================================
const unsigned long RESULT_DISPLAY_MS = 2000;
const unsigned long COOLDOWN_MS = 3000;

// ==================================================================
//  Shared state globals
// ==================================================================
AppMode appMode = HOME;
MatchSubMode matchSubMode = FP_MODE;
ScanState state = IDLE;

unsigned long resultStart = 0;
unsigned long cooldownStart = 0;

bool commandActive = false;
unsigned long commandDoneAt = 0;

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
  drawCommandScreen("Admin mode.\nWaiting for command...", TFT_CYAN);
}