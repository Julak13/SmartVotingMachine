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

    // 1) Admin-requested enrollment takes priority over idle waiting.
    bool wantEnroll;
    xSemaphoreTake(_enMtx, portMAX_DELAY);
    wantEnroll = _en.requested && !_en.active;
    xSemaphoreGive(_enMtx);
    if (wantEnroll) { runEnrollment(); continue; }

    // 2) Election must be open.
    if (store.status() != ELECTION_RUNNING) {
      _d->show("Election Closed", "Please wait...");
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // 3) Idle: wait for an NFC card.
    _d->show("Smart Voting", "Tap NFC Card");
    String uid = _nfc->readUID(NFC_READ_TIMEOUT_MS);
    if (uid.length() == 0) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

    handleVoter(uid);
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

  // --- fingerprint (second factor) ---
  _d->show("Card Accepted", "Place Finger");
  if (!runFingerprintCheck(expectedFid)) {
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
    while (millis() - t0 < 15000) {
      if (_ok->pressed()) { proceed = true; break; }
      vTaskDelay(pdMS_TO_TICKS(15));
    }
    if (!proceed) { _fb->denied(); flash("Session Timeout", "Try again", SCREEN_MSG_MS); return; }
  }

  // --- candidate selection ---
  int candId = selectCandidate();
  if (candId == 0) { flash("Vote Cancelled", "", SCREEN_MSG_MS); return; }

  // --- record (atomic, re-checks voted flag under lock) ---
  int rc = store.castVote(uid, expectedFid, (uint16_t)candId);
  switch (rc) {
    case 0: _fb->success(); flash("Vote Recorded", "Thank You", SCREEN_MSG_MS); break;
    case 2: _fb->denied();  flash("Already Voted", "Access Denied", SCREEN_MSG_MS); break;
    case 4: _fb->denied();  flash("Election Closed", "", SCREEN_MSG_MS); break;
    default: _fb->denied(); flash("Vote Failed", "Try again", SCREEN_MSG_MS); break;
  }
}

bool VotingController::runFingerprintCheck(uint16_t expectedFid) {
  uint32_t t0 = millis();
  while (millis() - t0 < ENROLL_FP_TIMEOUT_MS) {
    int id = _fp->search();
    if (id > 0)  return (uint16_t)id == expectedFid;
    if (id == 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }  // no finger yet
    // id < 0 -> finger seen but no good match
    return false;
  }
  return false;   // timed out
}

// ---------------------------------------------------------------------
//  Candidate selection via UP/DOWN/OK with a confirm step
// ---------------------------------------------------------------------
int VotingController::selectCandidate() {
  std::vector<Candidate> cands = store.snapshotCandidates();
  if (cands.empty()) { flash("No Candidates", "", SCREEN_MSG_MS); return 0; }

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

  for (;;) {
    if (_up->pressed())   { _fb->tick(); idx = (idx - 1 + n) % n; paint(); }
    if (_down->pressed()) { _fb->tick(); idx = (idx + 1) % n;     paint(); }
    if (_ok->pressed()) {
      // confirm
      _fb->tick();
      _d->show("Confirm vote?", cands[idx].name);
      vTaskDelay(pdMS_TO_TICKS(250));   // small guard so OK isn't double-read
      uint32_t t0 = millis();
      for (;;) {
        if (_ok->pressed())   return cands[idx].id;     // OK = confirm
        if (_up->pressed() || _down->pressed()) { paint(); break; } // cancel
        if (millis() - t0 > 12000) { paint(); break; }  // timeout -> back
        vTaskDelay(pdMS_TO_TICKS(15));
      }
    }
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
  if (!_nfc->ready()){ fail("NFC reader offline"); return; }

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
  String uid;
  {
    uint32_t t0 = millis();
    while (millis() - t0 < ENROLL_NFC_TIMEOUT_MS) {
      if (enrollCancelled()) { _fp->remove(fid); fail("Cancelled"); return; }
      uid = _nfc->readUID(300);
      if (uid.length()) break;
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
  if (uid.length() == 0) { _fp->remove(fid); fail("No card detected"); return; }
  if (store.uidExists(uid)) { _fp->remove(fid); fail("Card already registered"); return; }

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
