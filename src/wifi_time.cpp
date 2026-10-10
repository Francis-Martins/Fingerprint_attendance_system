#include "wifi_time.h"
#include "config.h"
#include "secrets.h"
#include <WiFi.h>
#include <time.h>

// ==================================================================
//  Time (NTP)
// ==================================================================
bool timeReady = false;

// ==================================================================
//  WiFi
// ==================================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.print("\nConnected. IP: ");
  Serial.println(WiFi.localIP());

  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
}

// ==================================================================
//  Time (NTP)
// ==================================================================
void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  unsigned long start = millis();
  while (!getLocalTime(&timeinfo) && millis() - start < 10000) {
    delay(200);
  }

  timeReady = getLocalTime(&timeinfo);
  if (timeReady) {
    Serial.println("Time synced.");
  } else {
    Serial.println("Time sync failed, Date/Time fields will be inaccurate.");
  }
}

String getDateString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "unknown";
  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
  return String(buf);
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "unknown";
  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  return String(buf);
}