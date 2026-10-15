#pragma once
// =====================================================================
//  NFCReader.h  -  PN532 NFC reader wrapper (I2C mode).
//  Reads the UID of an ISO14443A card (MIFARE Classic/NTAG/etc.) and
//  returns it as an uppercase hex string.
//
//  Self-healing: the reader is periodically asked for its firmware
//  version while idle. If it has stopped answering it is hardware-reset
//  through its RST line (GPIO5) and re-configured, so a stuck reader
//  recovers by itself instead of needing a power cycle.
// =====================================================================
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include "config.h"

class NFCReader {
public:
  bool begin();                 // false if module not detected
  bool ready() const { return _ok; }

  // Poll for a card. Returns the UID hex string, or "" if none seen.
  String readUID(uint16_t timeoutMs);

  // Call from the idle loop: every NFC_HEALTH_MS checks the chip still
  // answers and recovers it if not. Also brings up a reader that was
  // missing at boot.
  void maintain();

  // Immediate check + recovery. Returns ready().
  bool selfTest();

  uint32_t recoveries() const { return _recoveries; }

private:
  bool init();

  Adafruit_PN532 _nfc{PN532_IRQ, PN532_RESET};   // I2C constructor (uses Wire)
  bool     _ok         = false;
  uint32_t _lastCheck  = 0;
  uint32_t _recoveries = 0;
};
