#pragma once
// =====================================================================
//  Feedback.h  -  Voter-facing buzzer + status LEDs.
//  Owned by the voter task; patterns are short, self-contained and
//  block only briefly (via vTaskDelay) at discrete interaction points.
//    green LED -> session active / accredited / vote accepted
//    red   LED -> access denied / error
// =====================================================================
#include <Arduino.h>
#include "config.h"

class Feedback {
public:
  void begin();

  void startup();       // boot "ready" chime + LED lamp-test
  void idle();          // everything off (call between voters)
  void tick();          // tiny blip for button navigation (no LED change)
  void accredited();    // green on + single confirm beep
  void success();       // green on + double happy beep
  void denied();        // red on + one long low beep

  void greenOn(bool on);
  void redOn(bool on);

private:
  void buzz(uint16_t ms);
  inline void buzzerWrite(bool on);
};
