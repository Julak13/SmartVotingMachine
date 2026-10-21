// =====================================================================
//  NFCReader.cpp
//
//  v1.1.1: card reading is back to exactly what v1.0 did - configure the
//  reader once at boot, then only ever send card polls. v1.1.0 added a
//  bounded card search (setPassiveActivationRetries) and a periodic
//  getFirmwareVersion health check that hardware-reset the reader; on the
//  deployed unit that left the reader "not responding". Root cause not yet
//  confirmed on hardware, so the safe behaviour is restored. A reader that
//  is still missing is retried, but a working reader is never poked
//  between polls.
// =====================================================================
#include "NFCReader.h"

bool NFCReader::init() {
  _nfc.begin();                          // pulses RST, wakes the chip
  uint32_t ver = _nfc.getFirmwareVersion();
  if (!ver) { _ok = false; return false; }
  _nfc.SAMConfig();                      // configure to read RFID tags
  _ok = true;
  return true;
}

bool NFCReader::begin() {
  bool ok = init();
  _lastCheck = millis();
  if (ok) Serial.println(F("[NFC] PN532 OK"));
  else    Serial.println(F("[NFC] PN532 not found - check I2C wiring/address/DIP switches"));
  return ok;
}

bool NFCReader::selfTest() {
  if (_ok) return true;                  // never re-initialise a working reader
  _lastCheck = millis();
  bool ok = init();
  if (ok) { _recoveries++; Serial.println(F("[NFC] reader came online")); }
  return ok;
}

void NFCReader::maintain() {
  if (_ok) return;
  if (millis() - _lastCheck >= NFC_RETRY_MS) selfTest();
}

String NFCReader::readUID(uint16_t timeoutMs) {
  if (!_ok) return "";

  // ISO14443A UIDs are 4, 7 or 10 bytes and the library copies the full
  // length it receives, so the buffer must hold the largest case.
  uint8_t uid[10] = {0};
  uint8_t uidLen = 0;

  if (!_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, timeoutMs))
    return "";
  if (uidLen == 0 || uidLen > sizeof(uid)) return "";

  String s;
  for (uint8_t i = 0; i < uidLen; i++) {
    if (uid[i] < 0x10) s += '0';
    s += String(uid[i], HEX);
  }
  s.toUpperCase();
  return s;
}
