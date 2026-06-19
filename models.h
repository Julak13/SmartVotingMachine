#pragma once
// =====================================================================
//  models.h  -  Plain data structures shared across the whole project.
//  No logic here, just the shape of the data we persist and pass around.
// =====================================================================
#include <Arduino.h>

// A single candidate on the ballot.
struct Candidate {
  uint16_t id;        // stable id, never reused within an election
  String   name;
  uint32_t votes;
};

// A registered voter. Two independent factors (NFC UID + fingerprint id)
// must BOTH resolve to the same record before a ballot is issued.
struct Voter {
  String   name;
  uint16_t fid;       // AS608 fingerprint template id (1..127)
  String   uid;       // NFC card UID, uppercase hex, e.g. "04A1B2C3"
  bool     voted;
};

// Lifecycle of the election. Voting is only possible while RUNNING.
enum ElectionStatus : uint8_t {
  ELECTION_SETUP   = 0,   // being configured, not yet opened
  ELECTION_RUNNING = 1,   // polls open
  ELECTION_STOPPED = 2    // polls closed
};

inline const char* electionStatusStr(ElectionStatus s) {
  switch (s) {
    case ELECTION_RUNNING: return "running";
    case ELECTION_STOPPED: return "stopped";
    default:               return "setup";
  }
}
