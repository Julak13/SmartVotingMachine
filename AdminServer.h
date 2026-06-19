#pragma once
// =====================================================================
//  AdminServer.h  -  WiFi SoftAP + HTTP REST API + dashboard.
//  Runs on the web task (core 0, shared with the WiFi stack). It NEVER
//  touches the LCD/buttons/sensors directly - enrollment is delegated to
//  the VotingController via a thread-safe request/poll interface.
// =====================================================================
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "VotingController.h"

class AdminServer {
public:
  void begin(VotingController* vc);
  void handle();                 // call repeatedly in the web task loop

private:
  WebServer        _srv{80};
  VotingController* _vc = nullptr;
  String           _token;       // session token issued at login

  bool authed();
  void sendJson(int code, const String& body);
  bool readJson(JsonDocument& doc);

  void routes();
  // handlers
  void hRoot();
  void hLogin();
  void hOverview();
  void hCandidates();
  void hAddCandidate();
  void hEditCandidate();
  void hDelCandidate();
  void hVoters();
  void hDelVoter();
  void hEnroll();
  void hEnrollStatus();
  void hEnrollCancel();
  void hResults();
  void hElection(const String& action);
};

extern AdminServer adminServer;
