#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================================================================
//  Display
// ==================================================================
#define TFT_BACKLIGHT_PIN 21

// ==================================================================
//  Touch (XPT2046)
// ==================================================================
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

#define RAW_X_MIN 3868   
#define RAW_X_MAX 202    
#define RAW_Y_MIN 3863   
#define RAW_Y_MAX 267    

extern const unsigned long TOUCH_DEBOUNCE_MS;

// ==================================================================
//  Button geometry
// ==================================================================
struct Btn {
  int x, y, w, h;
  const char* label;
};

extern Btn homeButtons[2];
extern Btn menuButton;
extern Btn toggleButton;

// ==================================================================
//  Fingerprint
// ==================================================================
#define FP_RX 22
#define FP_TX 27

// ==================================================================
//  RFID (PN532, I2C)
// ==================================================================
#define RFID_SDA_PIN 4
#define RFID_SCL_PIN 17

// ==================================================================
//  Firebase / user cache
// ==================================================================
#define MAX_CACHED_USERS 100

// ==================================================================
//  Buzzer (passive)
// ==================================================================
#define BUZZER_PIN 16
#define BUZZER_CHANNEL 0              // only used on Arduino-ESP32 core 2.x
#define BUZZER_FREQ 2700              // Hz, most passive buzzers are loudest around 2-4 kHz
#define BUZZER_DURATION_MS 1000
// ==================================================================
//  NTP
// ==================================================================
extern const char* NTP_SERVER;
extern const long GMT_OFFSET_SEC;
extern const int DAYLIGHT_OFFSET_SEC;

// ==================================================================
//  Command / poll timing
// ==================================================================
extern const unsigned long COMMAND_POLL_MS;
extern const unsigned long RFID_POLL_INTERVAL_MS;
extern const unsigned long RFID_READ_TIMEOUT_MS;

#endif // CONFIG_H