#pragma once
// =====================================================================
//  NFCReader.h  -  PN532 NFC reader wrapper (I2C mode).
//  Reads the UID of an ISO14443A card (MIFARE Classic/NTAG/etc.) and
//  returns it as an uppercase hex string.
//
//  Configured once at boot. A reader that was not found is retried every
//  NFC_RETRY_MS; a working reader is never re-initialised or sent anything
//  but card polls (see NFCReader.cpp for why).
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

  // Call from the idle loop: retries a reader that is not ready yet.
  void maintain();

  // true if ready; otherwise tries to bring the reader up once.
  bool selfTest();

  uint32_t recoveries() const { return _recoveries; }

private:
  bool init();

  Adafruit_PN532 _nfc{PN532_IRQ, PN532_RESET};   // I2C constructor (uses Wire)
  bool     _ok         = false;
  uint32_t _lastCheck  = 0;
  uint32_t _recoveries = 0;
};
