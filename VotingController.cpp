// =====================================================================
//  VotingController.cpp
// =====================================================================
#include "VotingController.h"
#include "Store.h"
#include "config.h"
#include <ArduinoJson.h>

void VotingController::begin(Display* d, FpSensor* fp, NFCReader* nfc,
                             Button* up, Button* down, Button* ok, Feedback* fb) {
  _d = d; _fp = fp; _nfc = nfc; _up = up; _down = down; _ok = ok; _fb = fb;
  _enMtx = xSemaphoreCreateMutex();
}

// ---------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------
void VotingController::flash(const String& l1, const String& l2, uint32_t ms) {
  _d->show(l1, l2);
  vTaskDelay(pdMS_TO_TICKS(ms));
}

// ---------------------------------------------------------------------
//  Main task loop
// ---------------------------------------------------------------------
void VotingController::run() {
  for (;;) {
    _fb->idle();                 // clear LEDs/buzzer between interactions
    processTemplateDeletes();    // before enrollment, which may reuse a slot

    // 1) Admin-requested enrollment takes priority over idle waiting.
    bool wantEnroll;
    xSemaphoreTake(_enMtx, portMAX_DELAY);
    wantEnroll = _en.requested && !_en.active;
    xSemaphoreGive(_enMtx);
    if (wantEnroll) { runEnrollment(); continue; }

    // Keep the reader healthy in every election state, so it is ready the
    // moment polls open and for enrollment.
    _nfc->maintain();

    // 2) Election must be open.
    if (store.status() != ELECTION_RUNNING) {
      _d->show("Election Closed", "Please wait...");
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    if (!_nfc->ready()) {        // maintain() keeps trying to bring it back
      _d->show("Card reader", "not responding");
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    // 3) Idle: wait for an NFC card.
    String uid = _nfc->readUID(NFC_READ_TIMEOUT_MS);
    uint32_t now = millis();
    if (uid.length()) {
      if (uid == _lastUid && (now - _lastSeenMs) < CARD_REPEAT_IGNORE_MS) {
        _lastSeenMs = now;       // the card just used is still lying there
        _lingerMs   = now;
      } else {
        handleVoter(uid);
        _lastUid    = uid;
        _lastSeenMs = millis();
        _lingerMs   = 0;
        continue;
      }
    }
    // "Remove card" only while a used card is actually still on the reader.
    bool lingering = _lingerMs && (millis() - _lingerMs) < 700;
    _d->show("Smart Voting", lingering ? "Remove card" : "Tap NFC Card");
    if (!uid.length()) vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------
//  Fingerprint templates of deleted voters
// ---------------------------------------------------------------------
// Firmware before v1.1.0 never erased a deleted voter's template. If that
// person was enrolled again, the sensor holds two copies of their finger and
// may report the old slot, so they could never vote. Runs once at boot.
void VotingController::sweepOrphanTemplates() {
  if (!_fp->ready()) return;
  uint32_t voters = store.totalVoters();
  uint16_t stored = _fp->templateCount();
  // Only when the sensor holds more templates than there are voters - and
  // never with an empty voter list, which could equally mean voters.json
  // failed to load; wiping every fingerprint then would be unrecoverable.
  if (voters == 0 || stored <= voters) return;

  _d->show("Tidying sensor", "please wait...");
  for (uint16_t id = FP_MIN_ID; id <= FP_MAX_ID; ++id)
    if (!store.fidInUse(id)) _fp->remove(id);
  Serial.printf("[FP] %u templates for %u voters - cleared unused slots, now %u\n",
                stored, (unsigned)voters, _fp->templateCount());
}

void VotingController::queueTemplateDelete(uint16_t fid) {
  xSemaphoreTake(_enMtx, portMAX_DELAY);
  if (_tplN < TPL_QUEUE) _tplDel[_tplN++] = fid;
  xSemaphoreGive(_enMtx);
}

void VotingController::processTemplateDeletes() {
  uint16_t pending[TPL_QUEUE];
  uint8_t  n;
  xSemaphoreTake(_enMtx, portMAX_DELAY);
  n = _tplN;
  for (uint8_t i = 0; i < n; ++i) pending[i] = _tplDel[i];
  _tplN = 0;
  xSemaphoreGive(_enMtx);

  for (uint8_t i = 0; i < n; ++i) {
    // Never erase a slot that a voter owns again by now.
    if (!store.fidInUse(pending[i])) _fp->remove(pending[i]);
  }
}

// ---------------------------------------------------------------------
//  One voter interaction
// ---------------------------------------------------------------------
void VotingController::handleVoter(const String& uid) {
  // Take a snapshot to inspect this voter without holding the lock long.
  std::vector<Voter> voters = store.snapshotVoters();
  const Voter* found = nullptr;
  for (auto& x : voters) if (x.uid.equalsIgnoreCase(uid)) { found = &x; break; }

  if (!found) {
    _fb->denied();
    flash("Card Not Known", "Access Denied", SCREEN_MSG_MS);
    return;
  }
  if (found->voted) {
    _fb->denied();
    flash("Already Voted", "Access Denied", SCREEN_MSG_MS);
    return;
  }

  uint16_t expectedFid = found->fid;

  if (!_fp->ready()) {
    _fb->denied();
    flash("Finger sensor", "not responding", SCREEN_MSG_MS);
    return;
  }

  // --- fingerprint (second factor) ---
  _d->show("Card Accepted", "Place Finger");
  int fp = runFingerprintCheck(expectedFid);
  if (fp == FPCHK_TIMEOUT) {
    _fb->denied();
    flash("No finger read", "Tap card again", SCREEN_MSG_MS);
    return;
  }
  if (fp != FPCHK_OK) {
    _fb->denied();
    flash("Finger Mismatch", "Access Denied", SCREEN_MSG_MS);
    return;
  }

  // --- accredited ---
  _fb->accredited();
  _d->show("Accredited", "Press OK");
  // wait for OK (with a timeout so an abandoned session resets)
  {
    uint32_t t0 = millis();
    bool proceed = false;
    while (millis() - t0 < ACCREDIT_WAIT_MS) {
      if (_ok->pressed()) { proceed = true; break; }
      vTaskDelay(pdMS_TO_TICKS(15));
    }
    if (!proceed) { _fb->denied(); flash("Session Timeout", "Tap card again", SCREEN_MSG_MS); return; }
  }

  // --- candidate selection ---
  int candId = selectCandidate();
  if (candId == SELECT_TIMEOUT) {
    _fb->denied();
    flash("Session Timeout", "Tap card again", SCREEN_MSG_MS);
    return;
  }
  if (candId == SELECT_NONE) return;          // "No Candidates" already shown

  // --- record (atomic, re-checks voted flag under lock) ---
  int rc = store.castVote(uid, expectedFid, (uint16_t)candId);
  switch (rc) {
    case 0: _fb->success(); flash("Vote Recorded", "Thank You", SCREEN_MSG_MS); break;
    case 2: _fb->denied();  flash("Already Voted", "Access Denied", SCREEN_MSG_MS); break;
    case 4: _fb->denied();  flash("Election Closed", "", SCREEN_MSG_MS); break;
    default: _fb->denied(); flash("Vote Failed", "Try again", SCREEN_MSG_MS); break;
  }
}

// A smudged or off-centre touch is common on an optical sensor and says
// nothing about whether the finger is right, so it is simply re-imaged.
// Only a clean image that is not this voter's finger counts as an attempt.
int VotingController::runFingerprintCheck(uint16_t expectedFid) {
  uint8_t  attempts = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < FP_VERIFY_TIMEOUT_MS) {
    int id = _fp->search();
    if (id == FpSensor::FP_NO_FINGER || id == FpSensor::FP_BAD_IMAGE) {
      vTaskDelay(pdMS_TO_TICKS(60));
      continue;
    }
    if (id > 0 && (uint16_t)id == expectedFid) return FPCHK_OK;

    // Clean read, but not this voter's finger (or another voter's).
    if (++attempts >= FP_MAX_ATTEMPTS) return FPCHK_MISMATCH;
    _fb->tick();
    _d->show("Not matched", "Lift, try again");
    _fp->waitFingerRemoved(4000);
    _d->show("Card Accepted", "Place Finger");
    t0 = millis();                        // a fresh window for the next try
  }
  return FPCHK_TIMEOUT;
}

// ---------------------------------------------------------------------
//  Candidate selection via UP/DOWN/OK with a confirm step
// ---------------------------------------------------------------------
int VotingController::selectCandidate() {
  std::vector<Candidate> cands = store.snapshotCandidates();
  if (cands.empty()) { flash("No Candidates", "", SCREEN_MSG_MS); return SELECT_NONE; }

  int idx = 0;
  const int n = cands.size();

  // initial paint
  auto paint = [&]() {
    int top = idx;
    int bot = (idx + 1) % n;
    // show current as the highlighted (top) line, next below
    _d->showSelection(cands[top].name, (n > 1 ? cands[bot].name : String("")),
                      true);
  };
  paint();

  // Without this limit a voter who walks away leaves the task here forever:
  // the booth is dead for everyone after them and enrollment cannot start.
  uint32_t lastAct = millis();
  for (;;) {
    if (_up->pressed())   { _fb->tick(); idx = (idx - 1 + n) % n; paint(); lastAct = millis(); }
    if (_down->pressed()) { _fb->tick(); idx = (idx + 1) % n;     paint(); lastAct = millis(); }
    if (_ok->pressed()) {
      _fb->tick();
      lastAct = millis();
      _d->show(cands[idx].name, "OK=Yes UP/DN=No");
      vTaskDelay(pdMS_TO_TICKS(250));   // small guard so OK isn't double-read
      uint32_t t0 = millis();
      bool backOut = false;
      for (;;) {
        if (_ok->pressed())   return cands[idx].id;     // OK = confirm
        if (_up->pressed() || _down->pressed()) { _fb->tick(); backOut = true; break; }
        if (millis() - t0 > CONFIRM_WAIT_MS) break;     // back to the list
        vTaskDelay(pdMS_TO_TICKS(15));
      }
      paint();
      if (backOut) lastAct = millis();
    }
    if (millis() - lastAct > SESSION_IDLE_MS) return SELECT_TIMEOUT;
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}

// =====================================================================
//  ENROLLMENT  (initiated from dashboard, executed here)
// =====================================================================
bool VotingController::enrollRequest(const String& name, String& err) {
  if (name.length() == 0)            { err = "Name required"; return false; }
  if (name.length() > MAX_NAME_LEN)  { err = "Name too long"; return false; }

  uint16_t fid = store.nextFreeFid();
  if (fid == 0) { err = "No free fingerprint slots"; return false; }
  if (store.totalVoters() >= MAX_VOTERS) { err = "Voter limit reached"; return false; }

  xSemaphoreTake(_enMtx, portMAX_DELAY);
  if (_en.active) { xSemaphoreGive(_enMtx); err = "Enrollment already running"; return false; }
  _en = EnrollState();          // reset
  _en.requested = true;
  _en.name = name;
  _en.fid  = fid;
  _en.message = "Queued...";
  xSemaphoreGive(_enMtx);
  return true;
}

void VotingController::enrollCancel() {
  xSemaphoreTake(_enMtx, portMAX_DELAY);
  _en.cancel = true;
  xSemaphoreGive(_enMtx);
}

bool VotingController::enrollCancelled() {
  xSemaphoreTake(_enMtx, portMAX_DELAY);
  bool c = _en.cancel;
  xSemaphoreGive(_enMtx);
  return c;
}

void VotingController::setEnroll(int step, const String& msg) {
  xSemaphoreTake(_enMtx, portMAX_DELAY);
  _en.step = step;
  _en.message = msg;
  xSemaphoreGive(_enMtx);
}

String VotingController::enrollStatusJson() {
  JsonDocument doc;
  xSemaphoreTake(_enMtx, portMAX_DELAY);
  doc["active"]  = _en.active;
  doc["done"]    = _en.done;
  doc["success"] = _en.success;
  doc["step"]    = _en.step;
  doc["fid"]     = _en.fid;
  doc["name"]    = _en.name;
  doc["uid"]     = _en.uid;
  doc["message"] = _en.message;
  xSemaphoreGive(_enMtx);
  String out; serializeJson(doc, out); return out;
}

void VotingController::runEnrollment() {
  // mark active
  String name; uint16_t fid;
  xSemaphoreTake(_enMtx, portMAX_DELAY);
  _en.active = true; _en.requested = false; _en.done = false;
  _en.success = false; _en.cancel = false;
  name = _en.name; fid = _en.fid;
  xSemaphoreGive(_enMtx);

  _d->show("Enrolling voter", "Use dashboard");

  auto fail = [&](const String& m) {
    xSemaphoreTake(_enMtx, portMAX_DELAY);
    _en.active = false; _en.done = true; _en.success = false; _en.message = m;
    xSemaphoreGive(_enMtx);
    _fb->denied();
  };

  if (!_fp->ready()) { fail("Fingerprint sensor offline"); return; }
  // selfTest() resets a wedged reader rather than giving up on it.
  if (!_nfc->selfTest()) { fail("NFC reader is not responding - check its wiring and power"); return; }

  // ---- step 1: first finger image ----
  setEnroll(1, "Place finger on sensor");
  {
    uint32_t t0 = millis(); bool got = false;
    while (millis() - t0 < ENROLL_FP_TIMEOUT_MS) {
      if (enrollCancelled()) { fail("Cancelled"); return; }
      int p = _fp->captureImage();
      if (p == FINGERPRINT_OK) { got = true; break; }
      vTaskDelay(pdMS_TO_TICKS(60));
    }
    if (!got)                 { fail("Timeout waiting for finger"); return; }
    if (!_fp->toTemplate(1))  { fail("Could not read finger (1)"); return; }

    // Refuse a finger that is already enrolled. Two templates of one finger
    // leave that voter unable to vote: the sensor reports the other slot,
    // which never matches their card.
    int dup = _fp->findTemplate();
    if (dup > 0) {
      String who = store.voterNameForFid(dup);
      if (who.length()) {
        fail("This finger is already registered to " + who + ". Use a different finger or delete that voter.");
        return;
      }
      _fp->remove(dup);     // left behind by a deleted voter - clear it
    }
  }

  // ---- step 2: remove finger ----
  setEnroll(2, "Remove finger");
  if (!_fp->waitFingerRemoved(ENROLL_FP_TIMEOUT_MS)) { fail("Finger not removed"); return; }

  // ---- step 3: second finger image ----
  setEnroll(3, "Place same finger again");
  {
    uint32_t t0 = millis(); bool got = false;
    while (millis() - t0 < ENROLL_FP_TIMEOUT_MS) {
      if (enrollCancelled()) { fail("Cancelled"); return; }
      int p = _fp->captureImage();
      if (p == FINGERPRINT_OK) { got = true; break; }
      vTaskDelay(pdMS_TO_TICKS(60));
    }
    if (!got)                { fail("Timeout waiting for finger"); return; }
    if (!_fp->toTemplate(2)) { fail("Could not read finger (2)"); return; }
  }

  // ---- step 4: build + store model ----
  setEnroll(4, "Creating template");
  if (!_fp->createModel()) { fail("Fingers did not match - retry"); return; }
  if (!_fp->store(fid))    { fail("Could not save template"); return; }

  // ---- step 5: capture NFC UID ----
  setEnroll(5, "Tap the NFC card");
  // Make sure the reader is answering before asking anyone to tap; this
  // resets it if it has wedged since its last self-test.
  if (!_nfc->selfTest()) {
    _fp->remove(fid);
    fail("NFC reader is not responding - check its wiring and power");
    return;
  }
  String uid;
  {
    uint32_t t0 = millis();
    while (millis() - t0 < ENROLL_NFC_TIMEOUT_MS) {
      if (enrollCancelled()) { _fp->remove(fid); fail("Cancelled"); return; }
      uid = _nfc->readUID(NFC_READ_TIMEOUT_MS);
      if (uid.length()) break;
      _nfc->maintain();
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
  if (uid.length() == 0) {
    _fp->remove(fid);
    fail("No card detected - hold the card flat on the reader for a second");
    return;
  }
  String owner = store.voterNameForUid(uid);
  if (owner.length()) {
    _fp->remove(fid);
    fail("This card is already registered to " + owner +
         ". Delete that voter first, or use a different card.");
    return;
  }

  // The new voter's card is probably still on the reader. Without this,
  // if polls are open it would immediately start a voting session.
  _lastUid    = uid;
  _lastSeenMs = millis();

  // ---- commit voter record ----
  if (!store.addVoter(name, fid, uid)) {
    _fp->remove(fid);
    fail("Could not save voter");
    return;
  }

  xSemaphoreTake(_enMtx, portMAX_DELAY);
  _en.uid = uid; _en.active = false; _en.done = true; _en.success = true;
  _en.message = "Voter enrolled successfully";
  xSemaphoreGive(_enMtx);

  _fb->success();
  _d->show("Voter enrolled", name);
  vTaskDelay(pdMS_TO_TICKS(SCREEN_MSG_MS));
}
