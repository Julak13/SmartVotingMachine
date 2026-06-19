#pragma once
// =====================================================================
//  NFCReader.h  -  PN532 NFC reader wrapper (I2C mode).
//  Reads the UID of an ISO14443A card (MIFARE Classic/NTAG/etc.) and
//  returns it as an uppercase hex string.
// =====================================================================
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include "config.h"

class NFCReader {
public:
  bool begin();                 // false if module not detected
  bool ready() const { return _ok; }

  // Poll for a card. Returns the UID hex string, or "" if none seen
  // within timeoutMs. Non-blocking-ish: blocks up to timeoutMs.
  String readUID(uint16_t timeoutMs);

private:
  Adafruit_PN532 _nfc{PN532_IRQ, PN532_RESET};   // I2C constructor (uses Wire)
  bool _ok = false;
};
