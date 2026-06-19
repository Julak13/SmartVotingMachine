#pragma once
// =====================================================================
//  Store.h  -  Single source of truth for all persisted data.
//
//  Candidates, voters and the election state are kept in RAM (std::vector)
//  for fast access and mirrored to LittleFS (internal SPI flash) as JSON
//  so they survive power loss and reboot. No SD card, no external DB.
//
//  Thread-safety: the voter task (core 1) and the web task (core 0) both
//  touch this object, so every public method is guarded by a RECURSIVE
//  mutex. Compound operations on the caller side can wrap several calls
//  with lock()/unlock() and still be safe.
// =====================================================================
#include <vector>
#include "models.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Store {
public:
  void begin();                 // mount FS + load everything into RAM

  void lock();                  // recursive - safe to nest
  void unlock();

  // ---- election state ----
  ElectionStatus status();
  void           setStatus(ElectionStatus s);

  // ---- candidates ----
  bool addCandidate(const String& name, uint16_t& outId);
  bool editCandidate(uint16_t id, const String& name);
  bool deleteCandidate(uint16_t id);
  std::vector<Candidate> snapshotCandidates();

  // ---- voters ----
  bool addVoter(const String& name, uint16_t fid, const String& uid);
  bool deleteVoter(uint16_t fid);
  bool uidExists(const String& uid);
  uint16_t nextFreeFid();                 // 0 if full / none available
  std::vector<Voter> snapshotVoters();

  // ---- voting (called only by the voter task) ----
  // Returns: 0 ok, 1 no such voter, 2 already voted, 3 bad candidate,
  //          4 election not running.
  int castVote(const String& uid, uint16_t expectedFid, uint16_t candidateId);

  // ---- aggregate helpers ----
  uint32_t totalVoters();
  uint32_t totalVotes();
  uint32_t votedCount();

  // ---- election control ----
  void resetElection();         // zero vote counts + clear voted flags

private:
  std::vector<Candidate> _cands;
  std::vector<Voter>     _voters;
  ElectionStatus         _status   = ELECTION_SETUP;
  uint16_t               _nextCand = 1;
  SemaphoreHandle_t      _mtx      = nullptr;

  // internal (assume lock already held)
  Candidate* findCand(uint16_t id);
  Voter*     findVoterByFid(uint16_t fid);
  Voter*     findVoterByUid(const String& uid);

  void loadCandidates();
  void loadVoters();
  void loadState();
  void saveCandidates();
  void saveVoters();
  void saveState();
};

extern Store store;
