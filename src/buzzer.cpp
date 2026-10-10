#include "buzzer.h"
#include "config.h"

static bool buzzing = false;
static unsigned long buzzStartMs = 0;

static void buzzerWrite(uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BUZZER_PIN, duty);
#else
  ledcWrite(BUZZER_CHANNEL, duty);
#endif
}

void buzzerBegin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BUZZER_PIN, BUZZER_FREQ, 8);          // 8-bit resolution
#else
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
#endif
  buzzerWrite(0);
}

void buzzerStart() {
  buzzerWrite(128);              // 50% duty = square wave
  buzzing = true;
  buzzStartMs = millis();
}

void buzzerUpdate() {
  if (buzzing && millis() - buzzStartMs >= BUZZER_DURATION_MS) {
    buzzerWrite(0);
    buzzing = false;
  }
}