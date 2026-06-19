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
  //   0  -> no finger present
  //  -1  -> finger present but no/low-confidence match
  int  search();
  int  lastConfidence() const { return _conf; }

  // ---- enrollment primitives (driven step-by-step by the controller) --
  int  captureImage();           // FINGERPRINT_OK / NOFINGER / error code
  bool toTemplate(uint8_t slot); // 1 or 2
  bool createModel();            // combine slot 1+2
  bool store(uint16_t id);       // write template to flash slot <id>
  bool remove(uint16_t id);      // erase template
  bool waitFingerRemoved(uint32_t timeoutMs);

private:
  Adafruit_Fingerprint _f{&Serial2};
  bool _ok   = false;
  int  _conf = 0;
};
