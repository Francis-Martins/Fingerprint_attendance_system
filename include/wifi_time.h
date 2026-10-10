#ifndef WIFI_TIME_H
#define WIFI_TIME_H

#include <Arduino.h>

// ==================================================================
//  WiFi
// ==================================================================
void connectWiFi();

// ==================================================================
//  Time (NTP)
// ==================================================================
extern bool timeReady;

void setupTime();
String getDateString();
String getTimeString();

#endif // WIFI_TIME_H