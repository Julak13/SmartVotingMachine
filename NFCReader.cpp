// =====================================================================
//  NFCReader.cpp
// =====================================================================
#include "NFCReader.h"

bool NFCReader::init() {
  // begin() pulses RST (GPIO5) and wakes the chip, so this is also the
  // recovery path for a reader that has wedged. Wire.begin() was already
  // called with our pins in the .ino; the library's own begin is a no-op
  // on an already-started bus.
  _nfc.begin();
  uint32_t ver = _nfc.getFirmwareVersion();
  if (!ver) { _ok = false; return false; }

  _nfc.SAMConfig();
  // The library default is "search forever". Our host-side wait would then
  // expire first and leave the chip mid-command; the next command collides
  // with it and reads fail until they resync. A bounded search makes the
  // chip always answer (with "no card") before we stop waiting.
  _nfc.setPassiveActivationRetries(NFC_PASSIVE_RETRIES);
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
  _lastCheck = millis();
  if (_ok && _nfc.getFirmwareVersion()) return true;

  bool wasOk = _ok;
  bool ok = init();
  if (ok && wasOk) {
    _recoveries++;
    Serial.println(F("[NFC] reader stopped answering - reset and recovered"));
  } else if (ok) {
    Serial.println(F("[NFC] reader came online"));
  } else if (wasOk) {
    Serial.println(F("[NFC] reader not answering"));
  }
  return ok;
}

void NFCReader::maintain() {
  if (millis() - _lastCheck >= NFC_HEALTH_MS) selfTest();
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
