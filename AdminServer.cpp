// =====================================================================
//  AdminServer.cpp
// =====================================================================
#include "AdminServer.h"
#include "Store.h"
#include "config.h"
#include "web_assets.h"
#include <WiFi.h>

AdminServer adminServer;

// ---------------------------------------------------------------------
static String randomToken() {
  const char* hex = "0123456789abcdef";
  String t;
  for (int i = 0; i < 24; ++i) t += hex[esp_random() & 0x0F];
  return t;
}

// ---------------------------------------------------------------------
void AdminServer::begin(VotingController* vc) {
  _vc = vc;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CLIENTS);
  delay(100);
  Serial.printf("[Web] SoftAP \"%s\"  IP: %s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());

  routes();
  _srv.begin();
}

void AdminServer::handle() { _srv.handleClient(); }

// ---------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------
void AdminServer::sendJson(int code, const String& body) {
  _srv.send(code, "application/json", body);
}

bool AdminServer::readJson(JsonDocument& doc) {
  if (!_srv.hasArg("plain")) return false;
  return deserializeJson(doc, _srv.arg("plain")) == DeserializationError::Ok;
}

bool AdminServer::authed() {
  if (_token.length() == 0) return false;
  return _srv.hasArg("t") && _srv.arg("t") == _token;
}

// ---------------------------------------------------------------------
//  routing table
// ---------------------------------------------------------------------
void AdminServer::routes() {
  _srv.on("/",        HTTP_GET,  [this]{ hRoot(); });
  _srv.on("/api/login", HTTP_POST, [this]{ hLogin(); });

  _srv.on("/api/overview",   HTTP_GET,  [this]{ hOverview(); });
  _srv.on("/api/candidates", HTTP_GET,  [this]{ hCandidates(); });
  _srv.on("/api/candidates", HTTP_POST, [this]{ hAddCandidate(); });
  _srv.on("/api/candidates/edit",   HTTP_POST, [this]{ hEditCandidate(); });
  _srv.on("/api/candidates/delete", HTTP_POST, [this]{ hDelCandidate(); });

  _srv.on("/api/voters",        HTTP_GET,  [this]{ hVoters(); });
  _srv.on("/api/voters/delete", HTTP_POST, [this]{ hDelVoter(); });
  _srv.on("/api/voters/enroll", HTTP_POST, [this]{ hEnroll(); });
  _srv.on("/api/voters/enroll/status", HTTP_GET,  [this]{ hEnrollStatus(); });
  _srv.on("/api/voters/enroll/cancel", HTTP_POST, [this]{ hEnrollCancel(); });

  _srv.on("/api/results", HTTP_GET, [this]{ hResults(); });

  _srv.on("/api/election/start", HTTP_POST, [this]{ hElection("start"); });
  _srv.on("/api/election/stop",  HTTP_POST, [this]{ hElection("stop"); });
  _srv.on("/api/election/reset", HTTP_POST, [this]{ hElection("reset"); });

  _srv.onNotFound([this]{ _srv.send(404, "text/plain", "Not found"); });
}

// ---------------------------------------------------------------------
//  handlers
// ---------------------------------------------------------------------
void AdminServer::hRoot() {
  _srv.send_P(200, "text/html", DASHBOARD_HTML);
}

void AdminServer::hLogin() {
  JsonDocument body;
  if (!readJson(body)) { sendJson(400, "{\"ok\":false}"); return; }
  String pw = (const char*)(body["password"] | "");
  JsonDocument res;
  if (pw == ADMIN_PASSWORD) {
    _token = randomToken();
    res["ok"] = true; res["token"] = _token;
  } else {
    res["ok"] = false;
  }
  String out; serializeJson(res, out); sendJson(200, out);
}

void AdminServer::hOverview() {
  if (!authed()) { sendJson(401, "{}"); return; }
  uint32_t reg = store.totalVoters();
  uint32_t cast = store.votedCount();
  JsonDocument d;
  d["registered"] = reg;
  d["cast"]       = cast;
  d["remaining"]  = (reg > cast) ? (reg - cast) : 0;
  d["candidates"] = (uint32_t)store.snapshotCandidates().size();
  d["status"]     = electionStatusStr(store.status());
  String out; serializeJson(d, out); sendJson(200, out);
}

void AdminServer::hCandidates() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument d;
  JsonArray arr = d["candidates"].to<JsonArray>();
  for (auto& c : store.snapshotCandidates()) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = c.id; o["name"] = c.name; o["votes"] = c.votes;
  }
  String out; serializeJson(d, out); sendJson(200, out);
}

void AdminServer::hAddCandidate() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument body;
  if (!readJson(body)) { sendJson(400, "{\"ok\":false,\"error\":\"bad request\"}"); return; }
  String name = (const char*)(body["name"] | "");
  name.trim();
  uint16_t id;
  bool ok = store.addCandidate(name, id);
  JsonDocument res; res["ok"] = ok;
  if (!ok) res["error"] = "Could not add (limit reached or empty name)";
  String out; serializeJson(res, out); sendJson(200, out);
}

void AdminServer::hEditCandidate() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument body;
  if (!readJson(body)) { sendJson(400, "{\"ok\":false}"); return; }
  uint16_t id = (uint16_t)(body["id"] | 0);
  String name = (const char*)(body["name"] | "");
  name.trim();
  bool ok = store.editCandidate(id, name);
  JsonDocument res; res["ok"] = ok; if (!ok) res["error"] = "Edit failed";
  String out; serializeJson(res, out); sendJson(200, out);
}

void AdminServer::hDelCandidate() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument body;
  if (!readJson(body)) { sendJson(400, "{\"ok\":false}"); return; }
  uint16_t id = (uint16_t)(body["id"] | 0);
  bool ok = store.deleteCandidate(id);
  JsonDocument res; res["ok"] = ok; if (!ok) res["error"] = "Delete failed";
  String out; serializeJson(res, out); sendJson(200, out);
}

void AdminServer::hVoters() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument d;
  JsonArray arr = d["voters"].to<JsonArray>();
  for (auto& v : store.snapshotVoters()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = v.name; o["fid"] = v.fid; o["uid"] = v.uid; o["voted"] = v.voted;
  }
  String out; serializeJson(d, out); sendJson(200, out);
}

void AdminServer::hDelVoter() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument body;
  if (!readJson(body)) { sendJson(400, "{\"ok\":false}"); return; }
  uint16_t fid = (uint16_t)(body["fid"] | 0);
  bool ok = store.deleteVoter(fid);
  JsonDocument res; res["ok"] = ok; if (!ok) res["error"] = "Delete failed";
  String out; serializeJson(res, out); sendJson(200, out);
}

void AdminServer::hEnroll() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument body;
  if (!readJson(body)) { sendJson(400, "{\"ok\":false}"); return; }
  String name = (const char*)(body["name"] | "");
  name.trim();
  String err;
  bool ok = _vc->enrollRequest(name, err);
  JsonDocument res; res["ok"] = ok; if (!ok) res["error"] = err;
  String out; serializeJson(res, out); sendJson(200, out);
}

void AdminServer::hEnrollStatus() {
  if (!authed()) { sendJson(401, "{}"); return; }
  sendJson(200, _vc->enrollStatusJson());
}

void AdminServer::hEnrollCancel() {
  if (!authed()) { sendJson(401, "{}"); return; }
  _vc->enrollCancel();
  sendJson(200, "{\"ok\":true}");
}

void AdminServer::hResults() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument d;
  d["total"] = store.totalVotes();
  JsonArray arr = d["candidates"].to<JsonArray>();
  for (auto& c : store.snapshotCandidates()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = c.name; o["votes"] = c.votes;
  }
  String out; serializeJson(d, out); sendJson(200, out);
}

void AdminServer::hElection(const String& action) {
  if (!authed()) { sendJson(401, "{}"); return; }
  if      (action == "start") store.setStatus(ELECTION_RUNNING);
  else if (action == "stop")  store.setStatus(ELECTION_STOPPED);
  else if (action == "reset") store.resetElection();
  sendJson(200, "{\"ok\":true}");
}
