#pragma once
// =====================================================================
//  NFCReader.h  -  PN532 NFC reader wrapper (I2C mode).
//  Reads the UID of an ISO14443A card (MIFARE Classic/NTAG/etc.) and
//  returns it as an uppercase hex string.
//
//  Non-blocking, following the Adafruit library's own readMifareClassicIrq
//  example: arm ONE card search, check whether the reader has answered,
//  read the card, then arm again. A new command is never sent while a
//  search is still running - see NFCReader.cpp for why that matters.
// =====================================================================
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include "config.h"

class NFCReader {
public:
  bool begin();                 // false if module not detected
  bool ready() const { return _ok; }

  // Call repeatedly (every ~20 ms). Returns a card's UID once per
  // detection, otherwise "". Never blocks for more than ~100 ms.
  String poll();

  // Throw away a detection made while nobody was polling (e.g. a card
  // tapped during a voter session or while the election was closed), so
  // only a tap made from now on counts. A card still lying on the reader
  // is simply detected again.
  void flush();

  // Call from the idle loop: retries a reader that is not ready yet.
  void maintain();

  // true if ready; otherwise tries to bring the reader up once.
  bool selfTest();

  uint32_t recoveries() const { return _recoveries; }

private:
  bool   init();
  bool   responseReady();
  String takeCard();

  Adafruit_PN532 _nfc{PN532_IRQ, PN532_RESET};   // I2C constructor (uses Wire)
  bool     _ok         = false;
  bool     _armed      = false;   // a card search is running in the PN532
  uint32_t _disarmedAt = 0;
  uint32_t _lastCheck  = 0;
  uint32_t _recoveries = 0;
};
