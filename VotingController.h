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

  // A deleted voter's fingerprint template lives in the AS608, which only
  // this task may talk to. The web task queues the slot; run() erases it.
  void   queueTemplateDelete(uint16_t fid);

  // Boot-time: erase templates no voter owns (left by older firmware).
  // Call from setup(), before the tasks start.
  void   sweepOrphanTemplates();

  // Read-only hardware health for the dashboard (plain reads of flags).
  bool     nfcReady() const      { return _nfc && _nfc->ready(); }
  bool     fpReady()  const      { return _fp && _fp->ready(); }
  uint32_t nfcRecoveries() const { return _nfc ? _nfc->recoveries() : 0; }

private:
  Display*   _d   = nullptr;
  FpSensor*  _fp  = nullptr;
  NFCReader* _nfc = nullptr;
  Button*    _up  = nullptr;
  Button*    _down= nullptr;
  Button*    _ok  = nullptr;
  Feedback*  _fb  = nullptr;

  EnrollState       _en;
  SemaphoreHandle_t _enMtx = nullptr;   // guards _en and the template queue

  static const uint8_t TPL_QUEUE = 16;
  uint16_t _tplDel[TPL_QUEUE];
  uint8_t  _tplN = 0;
  void processTemplateDeletes();

  // A card still lying on the reader after its session must not start a
  // new one; it is ignored until it has been lifted for a moment.
  String   _lastUid;
  uint32_t _lastSeenMs = 0;

  uint32_t _lingerMs   = 0;   // last time a used card was seen still lying there

  // ---- voting flow ----
  static const int SELECT_NONE    = 0;    // no candidates configured
  static const int SELECT_TIMEOUT = -1;   // voter walked away
  static const int FPCHK_OK       = 1;
  static const int FPCHK_MISMATCH = 0;    // clean reads, not this voter
  static const int FPCHK_TIMEOUT  = -1;   // no usable finger in time
  void handleVoter(const String& uid);
  int  runFingerprintCheck(uint16_t expectedFid);   // FPCHK_*
  int  selectCandidate();        // candidate id (>0) or SELECT_NONE/TIMEOUT
  void flash(const String& l1, const String& l2, uint32_t ms);

  // ---- enrollment flow ----
  void runEnrollment();
  void setEnroll(int step, const String& msg);
  bool enrollCancelled();
};
