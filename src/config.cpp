#include "config.h"

// ==================================================================
//  Touch debounce
// ==================================================================
const unsigned long TOUCH_DEBOUNCE_MS = 250;

// ==================================================================
//  Button geometry
// ==================================================================
Btn homeButtons[2] = {
  {40,  80, 100, 70, "MATCH"},
  {180, 80, 100, 70, "ADMIN"}
};

Btn menuButton   = {260, 5, 55, 25, "MENU"};
Btn toggleButton = {195, 5, 60, 25, "MODE"};

// ==================================================================
//  NTP
// ==================================================================
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// ==================================================================
//  Command / poll timing
// ==================================================================
const unsigned long COMMAND_POLL_MS = 2500;
const unsigned long RFID_POLL_INTERVAL_MS = 150;
const unsigned long RFID_READ_TIMEOUT_MS = 50;