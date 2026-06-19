#pragma once
// =====================================================================
//  Buttons.h  -  Debounced active-LOW push button with edge detection.
//  Call update() often; pressed() returns true exactly once per push.
// =====================================================================
#include <Arduino.h>
#include "config.h"

class Button {
public:
  explicit Button(uint8_t pin) : _pin(pin) {}

  void begin() {
    pinMode(_pin, INPUT_PULLUP);
    _stable = _last = digitalRead(_pin);
    _tChange = millis();
  }

  // Must be polled regularly. Returns true on a clean press edge.
  bool pressed() {
    bool raw = digitalRead(_pin);
    uint32_t now = millis();
    if (raw != _last) { _last = raw; _tChange = now; }
    if ((now - _tChange) >= BTN_DEBOUNCE_MS && raw != _stable) {
      _stable = raw;
      if (_stable == LOW) return true;     // active LOW -> pressed
    }
    return false;
  }

private:
  uint8_t  _pin;
  bool     _stable = HIGH;
  bool     _last   = HIGH;
  uint32_t _tChange = 0;
};
