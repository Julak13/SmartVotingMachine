// =====================================================================
//  Store.cpp
// =====================================================================
#include "Store.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

Store store;   // global singleton

// ---- small RAII-ish lock guard -------------------------------------
struct Guard {
  Store* s; Guard(Store* st):s(st){ s->lock(); } ~Guard(){ s->unlock(); }
};

// ---------------------------------------------------------------------
void Store::begin() {
  _mtx = xSemaphoreCreateRecursiveMutex();

  if (!LittleFS.begin(true)) {           // true = format on first boot
    Serial.println(F("[Store] LittleFS mount failed!"));
    return;
  }
  loadState();
  loadCandidates();
  loadVoters();
  Serial.printf("[Store] loaded %u candidates, %u voters, status=%s\n",
                (unsigned)_cands.size(), (unsigned)_voters.size(),
                electionStatusStr(_status));
}

void Store::lock()   { if (_mtx) xSemaphoreTakeRecursive(_mtx, portMAX_DELAY); }
void Store::unlock() { if (_mtx) xSemaphoreGiveRecursive(_mtx); }

// ---------------------------------------------------------------------
//  Election state
// ---------------------------------------------------------------------
ElectionStatus Store::status() { Guard g(this); return _status; }

void Store::setStatus(ElectionStatus s) {
  Guard g(this);
  _status = s;
  saveState();
}

// ---------------------------------------------------------------------
//  Candidates
// ---------------------------------------------------------------------
Candidate* Store::findCand(uint16_t id) {
  for (auto& c : _cands) if (c.id == id) return &c;
  return nullptr;
}

bool Store::addCandidate(const String& name, uint16_t& outId) {
  Guard g(this);
  if (_cands.size() >= MAX_CANDIDATES) return false;
  if (name.length() == 0) return false;
  Candidate c; c.id = _nextCand++; c.name = name; c.votes = 0;
  outId = c.id;
  _cands.push_back(c);
  saveCandidates();
  saveState();                  // _nextCand changed
  return true;
}

bool Store::editCandidate(uint16_t id, const String& name) {
  Guard g(this);
  Candidate* c = findCand(id);
  if (!c || name.length() == 0) return false;
  c->name = name;
  saveCandidates();
  return true;
}

bool Store::deleteCandidate(uint16_t id) {
  Guard g(this);
  for (size_t i = 0; i < _cands.size(); ++i) {
    if (_cands[i].id == id) {
      _cands.erase(_cands.begin() + i);
      saveCandidates();
      return true;
    }
  }
  return false;
}

std::vector<Candidate> Store::snapshotCandidates() {
  Guard g(this); return _cands;            // copy by value
}

// ---------------------------------------------------------------------
//  Voters
// ---------------------------------------------------------------------
Voter* Store::findVoterByFid(uint16_t fid) {
  for (auto& v : _voters) if (v.fid == fid) return &v;
  return nullptr;
}
Voter* Store::findVoterByUid(const String& uid) {
  for (auto& v : _voters) if (v.uid.equalsIgnoreCase(uid)) return &v;
  return nullptr;
}

bool Store::uidExists(const String& uid) {
  Guard g(this); return findVoterByUid(uid) != nullptr;
}

uint16_t Store::nextFreeFid() {
  Guard g(this);
  for (uint16_t id = FP_MIN_ID; id <= FP_MAX_ID; ++id)
    if (!findVoterByFid(id)) return id;
  return 0;                                 // none free
}

bool Store::addVoter(const String& name, uint16_t fid, const String& uid) {
  Guard g(this);
  if (_voters.size() >= MAX_VOTERS) return false;
  if (name.length() == 0 || uid.length() == 0) return false;
  if (findVoterByFid(fid) || findVoterByUid(uid)) return false;
  Voter v; v.name = name; v.fid = fid; v.uid = uid; v.voted = false;
  _voters.push_back(v);
  saveVoters();
  return true;
}

bool Store::deleteVoter(uint16_t fid) {
  Guard g(this);
  for (size_t i = 0; i < _voters.size(); ++i) {
    if (_voters[i].fid == fid) {
      _voters.erase(_voters.begin() + i);
      saveVoters();
      return true;
    }
  }
  return false;
}

std::vector<Voter> Store::snapshotVoters() {
  Guard g(this); return _voters;
}

// ---------------------------------------------------------------------
//  Casting a vote  -  the security-critical path
// ---------------------------------------------------------------------
int Store::castVote(const String& uid, uint16_t expectedFid, uint16_t candidateId) {
  Guard g(this);
  if (_status != ELECTION_RUNNING) return 4;

  Voter* v = findVoterByUid(uid);
  if (!v) return 1;                          // unknown card
  if (v->fid != expectedFid) return 1;       // factors disagree -> reject
  if (v->voted) return 2;                    // already voted

  Candidate* c = findCand(candidateId);
  if (!c) return 3;

  c->votes++;
  v->voted = true;
  // Persist the voted flag FIRST: if power is lost between the two writes
  // the worst case is a counted vote with the voter still able to retry,
  // never a double count. Counts are saved immediately after.
  saveVoters();
  saveCandidates();
  return 0;
}

// ---------------------------------------------------------------------
//  Aggregates
// ---------------------------------------------------------------------
uint32_t Store::totalVoters() { Guard g(this); return _voters.size(); }

uint32_t Store::totalVotes() {
  Guard g(this);
  uint32_t t = 0; for (auto& c : _cands) t += c.votes; return t;
}

uint32_t Store::votedCount() {
  Guard g(this);
  uint32_t t = 0; for (auto& v : _voters) if (v.voted) t++; return t;
}

void Store::resetElection() {
  Guard g(this);
  for (auto& c : _cands)  c.votes = 0;
  for (auto& v : _voters) v.voted = false;
  _status = ELECTION_SETUP;
  saveCandidates();
  saveVoters();
  saveState();
}

// ---------------------------------------------------------------------
//  Persistence  (LittleFS + ArduinoJson v7)
// ---------------------------------------------------------------------
void Store::loadState() {
  File f = LittleFS.open(FILE_STATE, "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    _status   = (ElectionStatus) (uint8_t) (doc["status"]   | 0);
    _nextCand = (uint16_t)              (doc["nextCand"] | 1);
    if (_nextCand < 1) _nextCand = 1;
  }
  f.close();
}

void Store::saveState() {
  JsonDocument doc;
  doc["status"]   = (uint8_t)_status;
  doc["nextCand"] = _nextCand;
  File f = LittleFS.open(FILE_STATE, "w");
  if (f) { serializeJson(doc, f); f.close(); }
}

void Store::loadCandidates() {
  _cands.clear();
  File f = LittleFS.open(FILE_CANDIDATES, "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    for (JsonObject o : doc["candidates"].as<JsonArray>()) {
      Candidate c;
      c.id    = (uint16_t)(o["id"]    | 0);
      c.name  = (const char*)(o["name"] | "");
      c.votes = (uint32_t)(o["votes"] | 0);
      if (c.id) {
        _cands.push_back(c);
        if (c.id >= _nextCand) _nextCand = c.id + 1;
      }
    }
  }
  f.close();
}

void Store::saveCandidates() {
  JsonDocument doc;
  JsonArray arr = doc["candidates"].to<JsonArray>();
  for (auto& c : _cands) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = c.id; o["name"] = c.name; o["votes"] = c.votes;
  }
  File f = LittleFS.open(FILE_CANDIDATES, "w");
  if (f) { serializeJson(doc, f); f.close(); }
}

void Store::loadVoters() {
  _voters.clear();
  File f = LittleFS.open(FILE_VOTERS, "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    for (JsonObject o : doc["voters"].as<JsonArray>()) {
      Voter v;
      v.name  = (const char*)(o["name"] | "");
      v.fid   = (uint16_t)(o["fid"] | 0);
      v.uid   = (const char*)(o["uid"]  | "");
      v.voted = (bool)(o["voted"] | false);
      if (v.fid && v.uid.length()) _voters.push_back(v);
    }
  }
  f.close();
}

void Store::saveVoters() {
  JsonDocument doc;
  JsonArray arr = doc["voters"].to<JsonArray>();
  for (auto& v : _voters) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = v.name; o["fid"] = v.fid; o["uid"] = v.uid; o["voted"] = v.voted;
  }
  File f = LittleFS.open(FILE_VOTERS, "w");
  if (f) { serializeJson(doc, f); f.close(); }
}
