// =====================================================================
//  Feedback.cpp
// =====================================================================
#include "Feedback.h"

void Feedback::begin() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  idle();
}

inline void Feedback::buzzerWrite(bool on) {
#if BUZZER_ACTIVE_HIGH
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
#else
  digitalWrite(BUZZER_PIN, on ? LOW : HIGH);
#endif
}

void Feedback::buzz(uint16_t ms) {
  buzzerWrite(true);
  vTaskDelay(pdMS_TO_TICKS(ms));
  buzzerWrite(false);
}

void Feedback::greenOn(bool on) { digitalWrite(LED_GREEN, on ? HIGH : LOW); }
void Feedback::redOn(bool on)   { digitalWrite(LED_RED,   on ? HIGH : LOW); }

void Feedback::startup() {
  // Quick lamp-test: both LEDs on briefly, then a rising "ready" chime
  // (two short beeps + one longer) with the green LED held on.
  greenOn(true);
  redOn(true);
  vTaskDelay(pdMS_TO_TICKS(150));
  redOn(false);

  buzz(BEEP_SHORT_MS);
  vTaskDelay(pdMS_TO_TICKS(BEEP_GAP_MS));
  buzz(BEEP_SHORT_MS);
  vTaskDelay(pdMS_TO_TICKS(BEEP_GAP_MS));
  buzz(BEEP_SHORT_MS * 2);

  vTaskDelay(pdMS_TO_TICKS(150));
  greenOn(false);
}

void Feedback::idle() {
  greenOn(false);
  redOn(false);
  buzzerWrite(false);
}

void Feedback::tick() {
  buzz(25);                       // very short navigation blip
}

void Feedback::accredited() {
  redOn(false);
  greenOn(true);
  buzz(BEEP_SHORT_MS);            // single confirmation beep
}

void Feedback::success() {
  redOn(false);
  greenOn(true);
  buzz(BEEP_SHORT_MS);
  vTaskDelay(pdMS_TO_TICKS(BEEP_GAP_MS));
  buzz(BEEP_SHORT_MS);           // double "accepted" beep
}

void Feedback::denied() {
  greenOn(false);
  redOn(true);
  buzz(BEEP_LONG_MS);            // one long low buzz
}
