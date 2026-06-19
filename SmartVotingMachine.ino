// =====================================================================
//  SmartVotingMachine.ino  -  ESP32 DevKit V1
//
//  Secure, fully-offline electronic voting machine.
//    - Voter interface : 16x2 I2C LCD + UP/DOWN/OK buttons
//    - Authentication  : PN532 NFC card  +  AS608 fingerprint (two-factor)
//    - Admin dashboard : self-hosted WiFi SoftAP + responsive web UI
//    - Storage         : LittleFS (internal flash) - survives power loss
//
//  Two FreeRTOS tasks:
//    voterTask (core 1) - owns LCD/buttons/NFC/fingerprint + state machine
//    webTask   (core 0) - owns WiFi SoftAP + HTTP server (admin only)
//  They share data only through the mutex-guarded global `store`.
// =====================================================================
#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "Store.h"
#include "Display.h"
#include "Buttons.h"
#include "FpSensor.h"
#include "NFCReader.h"
#include "Feedback.h"
#include "VotingController.h"
#include "AdminServer.h"

// ---- hardware objects (owned by the voter task) ----
Display   display;
FpSensor  fingerprint;
NFCReader nfc;
Button    btnUp(BTN_UP);
Button    btnDown(BTN_DOWN);
Button    btnOk(BTN_OK);
Feedback  feedback;

VotingController voting;

// ---------------------------------------------------------------------
//  Task bodies
// ---------------------------------------------------------------------
void voterTask(void*) {
  voting.run();                  // never returns
}

void webTask(void*) {
  adminServer.begin(&voting);    // start SoftAP + HTTP server on core 0
  for (;;) {
    adminServer.handle();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// ---------------------------------------------------------------------
//  setup
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== Smart Voting Machine booting ==="));

  // --- shared peripherals ---
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);                 // LCD + PN532
  Serial2.begin(FP_BAUD, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN); // AS608

  // --- persistence first (so the UI has data immediately) ---
  store.begin();

  // --- voter-side hardware ---
  display.begin();
  display.show("Smart Voting", "Initializing...");

  btnUp.begin();
  btnDown.begin();
  btnOk.begin();
  feedback.begin();

  bool fpOk  = fingerprint.begin();
  bool nfcOk = nfc.begin();
  if (!fpOk || !nfcOk) {
    display.show(fpOk ? "NFC reader FAIL" : "Finger snsr FAIL",
                 "Check wiring");
    delay(2500);
  }

  voting.begin(&display, &fingerprint, &nfc, &btnUp, &btnDown, &btnOk, &feedback);

  // --- "system ready" chime + LED lamp-test (before tasks take over) ---
  display.show("Smart Voting", "System Ready");
  feedback.startup();

  // --- launch tasks ---
  xTaskCreatePinnedToCore(voterTask, "voter", 8192, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(webTask,   "web",   8192, nullptr, 2, nullptr, 0);

  Serial.println(F("=== Boot complete ==="));
}

// ---------------------------------------------------------------------
//  loop  -  unused; all work happens in the two tasks.
// ---------------------------------------------------------------------
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
