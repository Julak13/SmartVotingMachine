#pragma once
// =====================================================================
//  VotingController.h  -  The voter-facing state machine + the voter
//  enrollment workflow. Runs on its OWN FreeRTOS task (core 1) so it
//  fully owns the LCD, buttons, fingerprint sensor and NFC reader; the
//  web task never touches that hardware directly.
//
//  Enrollment is REQUESTED by the web task (admin dashboard) and EXECUTED
//  here, with progress published back through a mutex-guarded EnrollState
//  that the dashboard polls.
// =====================================================================
#include <Arduino.h>
#include "Display.h"
#include "Buttons.h"
#include "FpSensor.h"
#include "NFCReader.h"
#include "Feedback.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct EnrollState {
  bool    requested = false;   // admin asked to start
  bool    active    = false;   // controller is running it now
  bool    done      = false;   // finished (check success)
  bool    success   = false;
  bool    cancel    = false;   // admin asked to abort
  int     step      = 0;       // 0..5 progress
  uint16_t fid      = 0;
  String  name;
  String  uid;
  String  message   = "Idle";
};

class VotingController {
public:
  void begin(Display* d, FpSensor* fp, NFCReader* nfc,
             Button* up, Button* down, Button* ok, Feedback* fb);

  void run();                  // never returns - the task body

  // ---- called from the WEB task (thread-safe) ----
  bool   enrollRequest(const String& name, String& err);
  void   enrollCancel();
  String enrollStatusJson();

private:
  Display*   _d   = nullptr;
  FpSensor*  _fp  = nullptr;
  NFCReader* _nfc = nullptr;
  Button*    _up  = nullptr;
  Button*    _down= nullptr;
  Button*    _ok  = nullptr;
  Feedback*  _fb  = nullptr;

  EnrollState       _en;
  SemaphoreHandle_t _enMtx = nullptr;

  // ---- voting flow ----
  void handleVoter(const String& uid);
  bool runFingerprintCheck(uint16_t expectedFid);   // returns match ok
  int  selectCandidate();        // returns candidate id, or 0 if aborted
  void flash(const String& l1, const String& l2, uint32_t ms);

  // ---- enrollment flow ----
  void runEnrollment();
  void setEnroll(int step, const String& msg);
  bool enrollCancelled();
};
