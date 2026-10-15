#pragma once
// =====================================================================
//  Uplink.h  -  Optional internet connection, used ONLY to send election
//  results to Telegram.
//
//  The machine's own access point (dashboard + enrollment) is always up;
//  the router connection runs alongside it (WIFI_AP_STA). Voting never
//  depends on this link: if the internet is down nothing about casting or
//  counting votes changes, and an undelivered result is simply retried.
//
//  All network work happens on one worker task on core 0, next to the WiFi
//  stack, so a slow TLS handshake can never stall the web server or the
//  voting loop on core 1. Web handlers only queue requests and read status.
// =====================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

class Uplink {
public:
  void beginWifi();     // AP always; plus the router connection if configured
  void begin();         // start the worker task

  // ---- called from web handlers (thread-safe, never touch the network) ----
  void settingsJson(JsonObject out);     // secrets are masked
  void statusJson(JsonObject out);
  bool saveSettings(JsonDocument& in, String& err);
  bool requestTest(String& err);
  bool requestResults(String& err);
  bool requestDetectChat(String& err);
  bool requestScan(String& err);

  // ---- election hooks ----
  void electionStarted();
  void electionStopped();   // snapshots the final results immediately

private:
  enum Job : uint8_t { JOB_TEST = 1, JOB_RESULTS, JOB_START, JOB_DETECT,
                       JOB_SCAN, JOB_APPLY_WIFI };
  enum Sta : uint8_t { STA_OFF, STA_CONNECTING, STA_UP, STA_WAIT };

  struct Config {
    String wifiSsid, wifiPass;
    String tgToken, tgChat;
    bool   autoFinal   = true;   // send results when the election is stopped
    bool   notifyStart = true;   // announce when the polls open
  };

  Config            _cfg;
  SemaphoreHandle_t _mtx = nullptr;   // guards everything below and _cfg
  QueueHandle_t     _q   = nullptr;

  Sta      _sta      = STA_OFF;
  uint32_t _staT0    = 0;
  String   _staReason;

  bool     _busy     = false;
  bool     _everSent = false;
  bool     _lastOk   = false;
  String   _lastMsg  = "Nothing sent yet";
  uint32_t _lastMs   = 0;

  bool     _scanBusy = false;
  String   _scanJson = "[]";
  String   _detectMsg;

  // Final results are snapshotted when the election stops and re-sent until
  // delivered, so a reset or a later vote can never change what is reported.
  String   _pendingFinal;
  uint8_t  _finalTries  = 0;
  uint32_t _finalNextMs = 0;

  void  load();
  void  save();                 // caller holds _mtx
  bool  tgReady();              // token and chat both set
  bool  enqueue(uint8_t job);
  static void taskEntry(void* arg);
  void  worker();
  void  doJob(uint8_t job);
  void  serviceSta();
  void  serviceFinal();
  void  startSta();
  bool  ensureOnline(uint32_t waitMs);
  int   telegram(const char* method, const String& form, String* body);
  int   sendText(const String& text);
  void  setResult(bool ok, const String& msg);
  static String explain(int code);
  String buildResults(const char* heading, bool final);
  String buildStart();
};

extern Uplink uplink;
