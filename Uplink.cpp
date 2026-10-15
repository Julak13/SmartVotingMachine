// =====================================================================
//  Uplink.cpp
// =====================================================================
#include "Uplink.h"
#include "config.h"
#include "Store.h"
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>

Uplink uplink;

// Result codes below 0 are ours; 100..599 are HTTP statuses from Telegram.
static const int ERR_NOT_SET  = -1001;   // token or chat id missing
static const int ERR_CONNECT  = -1003;   // TLS connect to Telegram failed
static const int ERR_NO_REPLY = -1004;   // connected, but no HTTP reply
static const int ERR_OFFLINE  = -2000;   // no router connection

namespace {
struct Lock {
  SemaphoreHandle_t m;
  explicit Lock(SemaphoreHandle_t x) : m(x) { xSemaphoreTake(m, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(m); }
};
}  // namespace

static String urlEncode(const String& s) {
  String out;
  out.reserve(s.length() * 3 / 2);
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); out += b; }
  }
  return out;
}

// "12.5%" from a/b, rounded to one decimal place.
static String pct1(uint32_t a, uint32_t b) {
  if (!b) return "0%";
  uint32_t tenths = (a * 1000UL + b / 2) / b;
  return String(tenths / 10) + "." + String(tenths % 10) + "%";
}

// Telegram returns compact JSON, so the fields we need are found as text.
// That avoids parsing 64-bit group chat ids (e.g. -1001234567890) into a
// type that might not hold them.
static String jsonNumberAfter(const String& body, const char* key, int from = 0) {
  int k = body.indexOf(key, from);
  if (k < 0) return "";
  k += strlen(key);
  int e = k;
  while (e < (int)body.length() && (isdigit((uint8_t)body[e]) || body[e] == '-')) e++;
  return body.substring(k, e);
}
static String jsonStringAfter(const String& body, const char* key, int from) {
  int k = body.indexOf(key, from);
  if (k < 0) return "";
  k += strlen(key);
  int e = body.indexOf('"', k);
  return e > k ? body.substring(k, e) : String("");
}

// ---------------------------------------------------------------------
//  Persistence
// ---------------------------------------------------------------------
void Uplink::load() {
  File f = LittleFS.open(FILE_UPLINK, "r");
  if (!f) return;
  JsonDocument d;
  if (deserializeJson(d, f) == DeserializationError::Ok) {
    _cfg.wifiSsid    = (const char*)(d["wifiSsid"] | "");
    _cfg.wifiPass    = (const char*)(d["wifiPass"] | "");
    _cfg.tgToken     = (const char*)(d["tgToken"]  | "");
    _cfg.tgChat      = (const char*)(d["tgChat"]   | "");
    _cfg.autoFinal   = d["autoFinal"]   | true;
    _cfg.notifyStart = d["notifyStart"] | true;
  }
  f.close();
}

void Uplink::save() {
  JsonDocument d;
  d["wifiSsid"]    = _cfg.wifiSsid;
  d["wifiPass"]    = _cfg.wifiPass;
  d["tgToken"]     = _cfg.tgToken;
  d["tgChat"]      = _cfg.tgChat;
  d["autoFinal"]   = _cfg.autoFinal;
  d["notifyStart"] = _cfg.notifyStart;
  File f = LittleFS.open(FILE_UPLINK, "w");
  if (f) { serializeJson(d, f); f.close(); }
}

bool Uplink::tgReady() {
  Lock l(_mtx);
  return _cfg.tgToken.length() && _cfg.tgChat.length();
}

// ---------------------------------------------------------------------
//  WiFi: access point always, router connection alongside when configured
// ---------------------------------------------------------------------
void Uplink::beginWifi() {
  _mtx = xSemaphoreCreateMutex();
  load();                                  // LittleFS is mounted by store.begin()

  WiFi.persistent(false);                  // credentials live in /uplink.json
  WiFi.setHostname("votingmachine");
  bool sta = _cfg.wifiSsid.length() > 0;
  WiFi.mode(sta ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CLIENTS);
  delay(100);
  Serial.printf("[Web] SoftAP \"%s\"  IP: %s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());
  if (sta) startSta();
}

void Uplink::startSta() {
  String ssid, pass;
  { Lock l(_mtx); ssid = _cfg.wifiSsid; pass = _cfg.wifiPass; }
  if (!ssid.length()) { Lock l(_mtx); _sta = STA_OFF; _staReason = ""; return; }

  if (!(WiFi.getMode() & WIFI_MODE_STA)) WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[net] connecting to \"%s\"\n", ssid.c_str());

  Lock l(_mtx);
  _sta = STA_CONNECTING; _staT0 = millis(); _staReason = "";
}

void Uplink::serviceSta() {
  Sta s; uint32_t t0;
  { Lock l(_mtx); s = _sta; t0 = _staT0; }
  wl_status_t st = WiFi.status();
  uint32_t now = millis();

  switch (s) {
    case STA_OFF:
      break;
    case STA_CONNECTING:
      if (st == WL_CONNECTED) {
        Serial.printf("[net] online, ip=%s\n", WiFi.localIP().toString().c_str());
        Lock l(_mtx); _sta = STA_UP; _staReason = "";
      } else if (now - t0 > STA_CONNECT_TIMEOUT_MS) {
        const char* why = (st == WL_NO_SSID_AVAIL) ? "network not found"
                                                   : "check the network name and password";
        // Stop hunting for a router that isn't answering: continuous
        // reconnect scans hop channels and make the admin access point
        // stutter. Retry on a slow timer instead.
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false);
        Serial.printf("[net] not connected (%s); retrying in %lu s\n",
                      why, (unsigned long)(STA_RETRY_MS / 1000));
        Lock l(_mtx); _sta = STA_WAIT; _staT0 = now; _staReason = why;
      }
      break;
    case STA_UP:
      if (st != WL_CONNECTED) {
        Lock l(_mtx); _sta = STA_CONNECTING; _staT0 = now; _staReason = "connection lost";
      }
      break;
    case STA_WAIT:
      if (now - t0 > STA_RETRY_MS) startSta();
      break;
  }
}

bool Uplink::ensureOnline(uint32_t waitMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  Sta s;
  { Lock l(_mtx); s = _sta; }
  if (s == STA_OFF) return false;
  if (s == STA_WAIT) startSta();           // a send is worth an early retry
  uint32_t t0 = millis();
  while (millis() - t0 < waitMs) {
    if (WiFi.status() == WL_CONNECTED) {
      Lock l(_mtx); _sta = STA_UP; _staReason = "";
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  return false;
}

// ---------------------------------------------------------------------
//  Telegram transport - a hand-rolled HTTPS POST.
//  Not HTTPClient: this machine's sketchbook holds a stale core-2.x copy
//  that can outrank the core-bundled one and break the build on core 3.x.
// ---------------------------------------------------------------------
int Uplink::telegram(const char* method, const String& form, String* body) {
  String token;
  { Lock l(_mtx); token = _cfg.tgToken; }
  if (!token.length()) return ERR_NOT_SET;

  NetworkClientSecure* c = new NetworkClientSecure();
  if (!c) return ERR_CONNECT;
  // No certificate check. The bot token travels in this request, so someone
  // controlling the router could capture it and post messages as the bot.
  // It cannot read or change votes. Pinning Telegram's CA would close that
  // gap but would fail silently whenever Telegram rotates its certificates.
  c->setInsecure();
  c->setTimeout(10000);                    // ms (Stream::setTimeout)

  int code = ERR_CONNECT;
  if (c->connect("api.telegram.org", 443, 8000)) {
    String req;
    req.reserve(256 + form.length());
    // HTTP/1.0 so the reply is never chunked: the body simply runs until
    // the server closes the connection.
    req  = String("POST /bot") + token + "/" + method + " HTTP/1.0\r\n";
    req += "Host: api.telegram.org\r\n";
    req += "User-Agent: " FW_NAME "/" FW_VERSION "\r\n";
    req += "Content-Type: application/x-www-form-urlencoded\r\n";
    req += "Content-Length: " + String(form.length()) + "\r\n\r\n";
    req += form;
    c->print(req);

    String status = c->readStringUntil('\n');
    status.trim();
    int sp = status.indexOf(' ');
    code = (status.startsWith("HTTP/") && sp > 0) ? status.substring(sp + 1, sp + 4).toInt() : 0;
    if (code <= 0) {
      code = ERR_NO_REPLY;
    } else if (body) {
      for (;;) {                           // skip headers up to the blank line
        String h = c->readStringUntil('\n');
        if (h.length() <= 1) break;
      }
      uint32_t t0 = millis();
      while ((c->connected() || c->available()) && millis() - t0 < 8000 &&
             body->length() < 4096) {
        int ch = c->read();
        if (ch < 0) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
        *body += (char)ch;
      }
    }
  }
  c->stop();
  delete c;
  return code;
}

int Uplink::sendText(const String& text) {
  String chat;
  { Lock l(_mtx); chat = _cfg.tgChat; }
  if (!chat.length()) return ERR_NOT_SET;
  if (!ensureOnline(STA_CONNECT_TIMEOUT_MS)) return ERR_OFFLINE;
  String form = "chat_id=" + urlEncode(chat) + "&text=" + urlEncode(text);
  int code = telegram("sendMessage", form, nullptr);
  Serial.printf("[tg] sendMessage -> %d\n", code);
  return code;
}

String Uplink::explain(int code) {
  switch (code) {
    case 200:          return "Delivered";
    case 400:          return "Telegram rejected the chat ID. Open your bot, press START, then use Detect.";
    case 401:
    case 404:          return "Telegram rejected the bot token. Copy it again from @BotFather.";
    case 403:          return "The bot is not allowed to message this chat. Press START in the bot chat, or add the bot to the group.";
    case 409:          return "This bot has a webhook set. Remove it (deleteWebhook) and try again.";
    case 429:          return "Telegram rate limit. Wait a minute and try again.";
    case ERR_NOT_SET:  return "Set the bot token and chat ID first.";
    case ERR_OFFLINE:  return "No internet: the machine is not connected to the Wi-Fi router.";
    case ERR_CONNECT:  return "Could not reach Telegram. Does the router have internet?";
    case ERR_NO_REPLY: return "Telegram did not answer. Try again.";
    default:           return "Telegram error (" + String(code) + ")";
  }
}

void Uplink::setResult(bool ok, const String& msg) {
  Lock l(_mtx);
  _everSent = true; _lastOk = ok; _lastMsg = msg; _lastMs = millis();
}

// ---------------------------------------------------------------------
//  Messages
// ---------------------------------------------------------------------
static const char* statusWord(ElectionStatus s) {
  switch (s) {
    case ELECTION_RUNNING: return "Polls are OPEN";
    case ELECTION_STOPPED: return "Polls are CLOSED";
    default:               return "Election not started";
  }
}

String Uplink::buildResults(const char* heading, bool final) {
  std::vector<Candidate> c = store.snapshotCandidates();
  std::stable_sort(c.begin(), c.end(),
                   [](const Candidate& a, const Candidate& b) { return a.votes > b.votes; });
  uint32_t total = 0;
  for (auto& x : c) total += x.votes;
  uint32_t reg = store.totalVoters(), voted = store.votedCount();

  String s;
  s += "\xF0\x9F\x97\xB3 ";                       // ballot box
  s += heading; s += "\n";
  s += statusWord(store.status()); s += "\n\n";
  s += "Registered voters: " + String(reg) + "\n";
  s += "Votes cast: " + String(total);
  if (reg) s += " (turnout " + pct1(voted, reg) + ")";
  s += "\n\n";

  if (c.empty()) s += "No candidates configured.\n";
  for (size_t i = 0; i < c.size(); ++i) {
    s += String(i + 1) + ". " + c[i].name + " - " + String(c[i].votes) +
         (c[i].votes == 1 ? " vote" : " votes");
    if (total) s += " (" + pct1(c[i].votes, total) + ")";
    s += "\n";
  }

  if (total && !c.empty()) {
    uint32_t top = c[0].votes;
    String names; int n = 0;
    for (auto& x : c) if (x.votes == top) { if (n++) names += ", "; names += x.name; }
    s += "\n";
    if (n > 1)       s += "Tie between " + names + " (" + String(top) + " each)\n";
    else if (final)  s += "\xF0\x9F\x8F\x86 Winner: " + names + "\n";   // trophy
    else             s += "Leading: " + names + "\n";
  }
  s += "\nSmart Voting Machine v" FW_VERSION;
  return s;
}

String Uplink::buildStart() {
  std::vector<Candidate> c = store.snapshotCandidates();
  String s = "\xF0\x9F\x97\xB3 Polls are now OPEN\n\n";
  s += "Registered voters: " + String(store.totalVoters()) + "\n";
  s += "Candidates:\n";
  if (c.empty()) s += "(none configured)\n";
  for (auto& x : c) s += "- " + x.name + "\n";
  s += "\nResults will be sent here when the election is stopped.";
  return s;
}

// ---------------------------------------------------------------------
//  Worker
// ---------------------------------------------------------------------
void Uplink::begin() {
  _q = xQueueCreate(8, sizeof(uint8_t));
  // Core 0 beside the WiFi stack; TLS needs a deeper stack than most tasks.
  xTaskCreatePinnedToCore(taskEntry, "uplink", 10240, this, 1, nullptr, 0);
}

void Uplink::taskEntry(void* arg) { static_cast<Uplink*>(arg)->worker(); }

void Uplink::worker() {
  for (;;) {
    uint8_t job;
    if (xQueueReceive(_q, &job, pdMS_TO_TICKS(1000)) == pdTRUE) doJob(job);
    serviceSta();
    serviceFinal();
  }
}

bool Uplink::enqueue(uint8_t job) {
  return _q && xQueueSend(_q, &job, 0) == pdTRUE;
}

void Uplink::doJob(uint8_t job) {
  { Lock l(_mtx); _busy = true; }

  switch (job) {
    case JOB_APPLY_WIFI: {
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false);
      String ssid;
      { Lock l(_mtx); ssid = _cfg.wifiSsid; }
      if (ssid.length()) startSta();
      else { Lock l(_mtx); _sta = STA_OFF; _staReason = ""; }
      break;
    }

    case JOB_SCAN: {
      int n = WiFi.scanNetworks();           // ~2-4 s; enables the station side
      if (n < 0) {                           // busy mid-connect - try once more
        vTaskDelay(pdMS_TO_TICKS(1500));
        n = WiFi.scanNetworks();
      }
      JsonDocument d;
      JsonArray a = d.to<JsonArray>();
      if (n > 0) {
        std::vector<int> idx;
        for (int i = 0; i < n; i++) idx.push_back(i);
        std::sort(idx.begin(), idx.end(), [](int x, int y) { return WiFi.RSSI(x) > WiFi.RSSI(y); });
        std::vector<String> seen;
        for (int i : idx) {
          String s = WiFi.SSID(i);
          if (!s.length() || s == AP_SSID) continue;      // hidden / ourselves
          if (std::find(seen.begin(), seen.end(), s) != seen.end()) continue;
          seen.push_back(s);
          JsonObject o = a.add<JsonObject>();
          o["ssid"] = s;
          o["rssi"] = WiFi.RSSI(i);
          o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
          if (seen.size() >= 15) break;
        }
      }
      WiFi.scanDelete();
      String out; serializeJson(d, out);
      Lock l(_mtx); _scanJson = out; _scanBusy = false;
      break;
    }

    case JOB_DETECT: {
      String msg;
      if (!ensureOnline(STA_CONNECT_TIMEOUT_MS)) {
        msg = explain(ERR_OFFLINE);
      } else {
        String body;
        // The newest update only. The chat id is the person (or group) who
        // last messaged the bot - i.e. whoever just pressed START.
        int code = telegram("getUpdates", "offset=-1&limit=1", &body);
        if (code != 200) {
          msg = explain(code);
        } else {
          int k = body.indexOf("\"chat\":{\"id\":");
          String id = jsonNumberAfter(body, "\"chat\":{\"id\":");
          if (!id.length()) {
            msg = "No messages yet. Open your bot in Telegram, press START (or send it any message), then tap Detect again.";
          } else {
            String who = jsonStringAfter(body, "\"title\":\"", k);
            if (!who.length()) who = jsonStringAfter(body, "\"first_name\":\"", k);
            { Lock l(_mtx); _cfg.tgChat = id; save(); }
            msg = "Found " + (who.length() ? who : String("chat")) + " (" + id + ") and saved it.";
          }
        }
      }
      Lock l(_mtx); _detectMsg = msg;
      break;
    }

    case JOB_TEST: {
      int c = sendText("\xE2\x9C\x85 Test from the Smart Voting Machine.\n"
                       "Telegram is set up correctly - election results will be sent to this chat.");
      setResult(c == 200, c == 200 ? String("Test message delivered") : explain(c));
      break;
    }

    case JOB_RESULTS: {
      int c = sendText(buildResults("Election results (current)", false));
      setResult(c == 200, c == 200 ? String("Results sent to Telegram") : explain(c));
      break;
    }

    case JOB_START: {
      int c = sendText(buildStart());
      setResult(c == 200, c == 200 ? String("Polls-open notice sent") : explain(c));
      break;
    }
  }

  { Lock l(_mtx); _busy = false; }
}

void Uplink::serviceFinal() {
  String text;
  {
    Lock l(_mtx);
    if (!_pendingFinal.length() || millis() < _finalNextMs) return;
    text = _pendingFinal;
  }
  { Lock l(_mtx); _busy = true; }
  int c = sendText(text);
  Lock l(_mtx);
  _busy = false;
  if (c == 200) {
    _pendingFinal = "";
    _everSent = true; _lastOk = true; _lastMsg = "Final results delivered"; _lastMs = millis();
    return;
  }
  bool permanent = (c >= 400 && c < 500 && c != 429) || c == ERR_NOT_SET;
  if (permanent || ++_finalTries >= TG_MAX_RETRIES) {
    _pendingFinal = "";
    _everSent = true; _lastOk = false; _lastMs = millis();
    _lastMsg = explain(c) + " Final results were NOT sent - use 'Send results now'.";
  } else {
    _finalNextMs = millis() + TG_RETRY_MS;
    _everSent = true; _lastOk = false; _lastMs = millis();
    _lastMsg = explain(c) + " Final results will be retried every minute.";
  }
}

// ---------------------------------------------------------------------
//  Requests from the web task
// ---------------------------------------------------------------------
bool Uplink::requestTest(String& err) {
  if (!tgReady()) { err = explain(ERR_NOT_SET); return false; }
  if (!enqueue(JOB_TEST)) { err = "Busy - try again in a moment"; return false; }
  return true;
}

bool Uplink::requestResults(String& err) {
  if (!tgReady()) { err = explain(ERR_NOT_SET); return false; }
  if (!enqueue(JOB_RESULTS)) { err = "Busy - try again in a moment"; return false; }
  return true;
}

bool Uplink::requestDetectChat(String& err) {
  { Lock l(_mtx);
    if (!_cfg.tgToken.length()) { err = "Save the bot token first."; return false; }
    _detectMsg = "Looking for your chat...";
  }
  if (!enqueue(JOB_DETECT)) { err = "Busy - try again in a moment"; return false; }
  return true;
}

bool Uplink::requestScan(String& err) {
  { Lock l(_mtx);
    if (_scanBusy) return true;
    _scanBusy = true;
  }
  if (!enqueue(JOB_SCAN)) {
    Lock l(_mtx); _scanBusy = false;
    err = "Busy - try again in a moment";
    return false;
  }
  return true;
}

void Uplink::electionStarted() {
  bool go;
  { Lock l(_mtx); go = _cfg.notifyStart; }
  if (go && tgReady()) enqueue(JOB_START);
}

void Uplink::electionStopped() {
  bool go;
  { Lock l(_mtx); go = _cfg.autoFinal; }
  if (!go || !tgReady()) return;
  String text = buildResults("FINAL election results", true);
  Lock l(_mtx);
  _pendingFinal = text; _finalTries = 0; _finalNextMs = 0;
}

// ---------------------------------------------------------------------
//  Settings
// ---------------------------------------------------------------------
static bool validChat(const String& c) {
  if (!c.length()) return true;
  if (c[0] == '@') return c.length() > 1;          // public channel username
  for (size_t i = 0; i < c.length(); i++)
    if (!(isdigit((uint8_t)c[i]) || (i == 0 && c[i] == '-'))) return false;
  return true;
}

bool Uplink::saveSettings(JsonDocument& in, String& err) {
  String ssid, pass, tok, chat;
  bool clearTg;
  {
    Lock l(_mtx);
    ssid = (const char*)(in["wifiSsid"] | _cfg.wifiSsid.c_str());
    chat = (const char*)(in["tgChat"]   | _cfg.tgChat.c_str());
  }
  pass    = (const char*)(in["wifiPass"] | "");
  tok     = (const char*)(in["tgToken"]  | "");
  clearTg = in["clearTelegram"] | false;
  ssid.trim(); chat.trim(); tok.trim();

  if (ssid.length() > 32) { err = "Wi-Fi name is too long"; return false; }
  if (pass.length() && (pass.length() < 8 || pass.length() > 63)) {
    err = "Wi-Fi password must be 8-63 characters"; return false;
  }
  if (tok.length() && (tok.indexOf(':') < 1 || tok.length() < 30)) {
    err = "That does not look like a bot token. It looks like 123456789:AAH... (copy it from @BotFather)";
    return false;
  }
  if (!validChat(chat)) { err = "Chat ID must be a number (e.g. 123456789 or -100...)"; return false; }

  bool wifiChanged;
  {
    Lock l(_mtx);
    bool ssidChanged = ssid != _cfg.wifiSsid;
    // A blank password keeps the saved one, unless the network changed -
    // then blank means an open network.
    String newPass = pass.length() ? pass : (ssidChanged ? String("") : _cfg.wifiPass);
    if (!ssid.length()) newPass = "";
    wifiChanged = ssidChanged || newPass != _cfg.wifiPass;
    _cfg.wifiSsid = ssid;
    _cfg.wifiPass = newPass;

    if (clearTg) { _cfg.tgToken = ""; _cfg.tgChat = ""; }
    else {
      if (tok.length()) _cfg.tgToken = tok;
      _cfg.tgChat = chat;
    }
    if (in["autoFinal"].is<bool>())   _cfg.autoFinal   = in["autoFinal"].as<bool>();
    if (in["notifyStart"].is<bool>()) _cfg.notifyStart = in["notifyStart"].as<bool>();
    save();
  }
  if (wifiChanged) enqueue(JOB_APPLY_WIFI);
  return true;
}

void Uplink::settingsJson(JsonObject out) {
  Lock l(_mtx);
  out["wifiSsid"]    = _cfg.wifiSsid;
  out["wifiPassSet"] = _cfg.wifiPass.length() > 0;
  out["tgTokenSet"]  = _cfg.tgToken.length() > 0;
  if (_cfg.tgToken.length() > 12)
    out["tgTokenHint"] = _cfg.tgToken.substring(0, 6) + "..." +
                         _cfg.tgToken.substring(_cfg.tgToken.length() - 4);
  out["tgChat"]      = _cfg.tgChat;
  out["autoFinal"]   = _cfg.autoFinal;
  out["notifyStart"] = _cfg.notifyStart;
}

void Uplink::statusJson(JsonObject out) {
  bool connected = WiFi.status() == WL_CONNECTED;
  Lock l(_mtx);
  JsonObject w = out["wifi"].to<JsonObject>();
  const char* st = "off";
  if (_sta == STA_CONNECTING) st = "connecting";
  else if (_sta == STA_UP && connected) st = "online";
  else if (_sta == STA_UP) st = "connecting";
  else if (_sta == STA_WAIT) st = "failed";
  w["state"]  = st;
  w["ssid"]   = _cfg.wifiSsid;
  w["reason"] = _staReason;
  if (connected) {
    w["ip"]   = WiFi.localIP().toString();
    w["rssi"] = WiFi.RSSI();
  }
  if (_sta == STA_WAIT) {
    uint32_t el = millis() - _staT0;
    w["retryIn"] = el < STA_RETRY_MS ? (STA_RETRY_MS - el) / 1000 : 0;
  }

  JsonObject t = out["telegram"].to<JsonObject>();
  t["ready"]        = _cfg.tgToken.length() && _cfg.tgChat.length();
  t["busy"]         = _busy;
  t["lastOk"]       = _lastOk;
  t["lastMsg"]      = _lastMsg;
  t["lastAgo"]      = _everSent ? (long)((millis() - _lastMs) / 1000) : -1L;
  t["pendingFinal"] = _pendingFinal.length() > 0;
  t["detectMsg"]    = _detectMsg;

  out["scanBusy"] = _scanBusy;
  out["networks"] = serialized(_scanJson);
}
