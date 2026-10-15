#pragma once
// =====================================================================
//  FpSensor.h  -  AS608 optical fingerprint sensor wrapper.
//  Wraps Adafruit_Fingerprint and exposes only the operations this
//  project needs: 1:N search (verify) and the multi-step enrollment.
// =====================================================================
#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include "config.h"

class FpSensor {
public:
  bool begin();                  // returns false if sensor not detected
  bool ready() const { return _ok; }
  uint16_t templateCount();

  // 1:N identification. Returns matched template id (>0) or:
  //   FP_NO_FINGER   no finger present
  //   FP_NO_MATCH    a clean image that matches no template (or matches
  //                  below FP_MIN_CONFIDENCE) - a real "no"
  //   FP_BAD_IMAGE   smudged / partial / comms hiccup - try again, this is
  //                  NOT evidence that the finger is wrong
  static constexpr int FP_NO_FINGER = 0;
  static constexpr int FP_NO_MATCH  = -1;
  static constexpr int FP_BAD_IMAGE = -2;
  int  search();
  int  lastConfidence() const { return _conf; }

  // ---- enrollment primitives (driven step-by-step by the controller) --
  int  captureImage();           // FINGERPRINT_OK / NOFINGER / error code
  bool toTemplate(uint8_t slot); // 1 or 2
  // After toTemplate(1): id of a stored template matching that finger,
  // or 0 if the finger is not enrolled yet.
  int  findTemplate();
  bool createModel();            // combine slot 1+2
  bool store(uint16_t id);       // write template to flash slot <id>
  bool remove(uint16_t id);      // erase template
  bool waitFingerRemoved(uint32_t timeoutMs);

private:
  Adafruit_Fingerprint _f{&Serial2};
  bool _ok   = false;
  int  _conf = 0;
};
