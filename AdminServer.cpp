// =====================================================================
//  AdminServer.cpp
// =====================================================================
#include "AdminServer.h"
#include "Store.h"
#include "Uplink.h"
#include "config.h"
#include "web_assets.h"
#include "web_icon.h"
#include <WiFi.h>
#include "esp_system.h"

AdminServer adminServer;

static const char* resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "Power on";
    case ESP_RST_BROWNOUT:  return "Brownout - the power supply dipped. Use a stronger 5 V supply.";
    case ESP_RST_PANIC:     return "Crash (software panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "Watchdog - the firmware stopped responding";
    case ESP_RST_SW:        return "Software restart";
    case ESP_RST_EXT:       return "Reset button";
    case ESP_RST_DEEPSLEEP: return "Wake from sleep";
    default:                return "Unknown";
  }
}

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

  uplink.beginWifi();          // own access point, plus router if configured
  routes();
  _srv.begin();
  uplink.begin();              // Telegram / router worker on core 0
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

  // "Add to Home Screen" support - public, contain nothing sensitive
  _srv.on("/manifest.json", HTTP_GET, [this]{ hManifest(); });
  _srv.on("/icon.png",      HTTP_GET, [this]{ hIcon(); });

  _srv.on("/api/device",         HTTP_GET,  [this]{ hDevice(); });
  _srv.on("/api/settings",       HTTP_GET,  [this]{ hSettingsGet(); });
  _srv.on("/api/settings",       HTTP_POST, [this]{ hSettingsSave(); });
  _srv.on("/api/uplink/status",  HTTP_GET,  [this]{ hUplinkStatus(); });
  _srv.on("/api/telegram/test",  HTTP_POST, [this]{ hTgTest(); });
  _srv.on("/api/telegram/send",  HTTP_POST, [this]{ hTgSend(); });
  _srv.on("/api/telegram/detect",HTTP_POST, [this]{ hTgDetect(); });
  _srv.on("/api/wifi/scan",      HTTP_POST, [this]{ hWifiScan(); });

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
  if (ok) _vc->queueTemplateDelete(fid);   // erase their fingerprint too
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
  ElectionStatus before = store.status();
  if (action == "start") {
    store.setStatus(ELECTION_RUNNING);
    if (before != ELECTION_RUNNING) uplink.electionStarted();
  } else if (action == "stop") {
    store.setStatus(ELECTION_STOPPED);
    // Only on a real transition, so pressing Stop twice can't send twice.
    if (before == ELECTION_RUNNING) uplink.electionStopped();
  } else if (action == "reset") {
    store.resetElection();
  }
  sendJson(200, "{\"ok\":true}");
}

// ---------------------------------------------------------------------
//  Home-screen ("install as app") support
// ---------------------------------------------------------------------
void AdminServer::hManifest() {
  _srv.send(200, "application/manifest+json",
    "{\"name\":\"Voting Machine Admin\",\"short_name\":\"Voting\","
    "\"start_url\":\"/\",\"display\":\"standalone\","
    "\"background_color\":\"#0b1020\",\"theme_color\":\"#0b1020\","
    "\"icons\":[{\"src\":\"/icon.png\",\"sizes\":\"192x192\",\"type\":\"image/png\"}]}");
}

void AdminServer::hIcon() {
  _srv.sendHeader("Cache-Control", "max-age=604800");
  _srv.send_P(200, "image/png", (const char*)ICON_PNG, ICON_PNG_LEN);
}

// ---------------------------------------------------------------------
//  Device health - the first thing to check when someone reports the
//  machine as "slow" or "unresponsive" (a brownout shows up here).
// ---------------------------------------------------------------------
void AdminServer::hDevice() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument d;
  d["fw"]            = FW_VERSION;
  d["uptime"]        = (uint32_t)(millis() / 1000);
  d["reset"]         = resetReasonStr();
  d["heap"]          = ESP.getFreeHeap();
  d["minHeap"]       = ESP.getMinFreeHeap();
  d["nfc"]           = _vc->nfcReady();
  d["fp"]            = _vc->fpReady();
  d["nfcRecoveries"] = _vc->nfcRecoveries();
  d["apClients"]     = WiFi.softAPgetStationNum();
  String out; serializeJson(d, out); sendJson(200, out);
}

// ---------------------------------------------------------------------
//  Internet + Telegram
// ---------------------------------------------------------------------
void AdminServer::sendOkOrError(bool ok, const String& err) {
  JsonDocument res; res["ok"] = ok; if (!ok) res["error"] = err;
  String out; serializeJson(res, out); sendJson(200, out);
}

void AdminServer::hSettingsGet() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument d;
  uplink.settingsJson(d.to<JsonObject>());
  String out; serializeJson(d, out); sendJson(200, out);
}

void AdminServer::hSettingsSave() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument body;
  if (!readJson(body)) { sendJson(400, "{\"ok\":false,\"error\":\"bad request\"}"); return; }
  String err;
  sendOkOrError(uplink.saveSettings(body, err), err);
}

void AdminServer::hUplinkStatus() {
  if (!authed()) { sendJson(401, "{}"); return; }
  JsonDocument d;
  uplink.statusJson(d.to<JsonObject>());
  String out; serializeJson(d, out); sendJson(200, out);
}

void AdminServer::hTgTest() {
  if (!authed()) { sendJson(401, "{}"); return; }
  String err; sendOkOrError(uplink.requestTest(err), err);
}

void AdminServer::hTgSend() {
  if (!authed()) { sendJson(401, "{}"); return; }
  String err; sendOkOrError(uplink.requestResults(err), err);
}

void AdminServer::hTgDetect() {
  if (!authed()) { sendJson(401, "{}"); return; }
  String err; sendOkOrError(uplink.requestDetectChat(err), err);
}

void AdminServer::hWifiScan() {
  if (!authed()) { sendJson(401, "{}"); return; }
  String err; sendOkOrError(uplink.requestScan(err), err);
}
