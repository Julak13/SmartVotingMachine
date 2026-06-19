// =====================================================================
//  NFCReader.cpp
// =====================================================================
#include "NFCReader.h"

bool NFCReader::begin() {
  // Wire.begin() is called once in the .ino before this.
  _nfc.begin();
  uint32_t ver = _nfc.getFirmwareVersion();
  if (!ver) {
    Serial.println(F("[NFC] PN532 not found - check I2C wiring/address"));
    _ok = false;
    return false;
  }
  _nfc.SAMConfig();             // configure to read RFID tags
  _ok = true;
  Serial.printf("[NFC] PN532 firmware 0x%08X OK\n", ver);
  return true;
}

String NFCReader::readUID(uint16_t timeoutMs) {
  if (!_ok) return "";

  uint8_t uid[7] = {0};
  uint8_t uidLen = 0;

  // readPassiveTargetID blocks up to timeoutMs waiting for a tag.
  if (!_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, timeoutMs))
    return "";
  if (uidLen == 0) return "";

  String s;
  for (uint8_t i = 0; i < uidLen; i++) {
    if (uid[i] < 0x10) s += '0';
    s += String(uid[i], HEX);
  }
  s.toUpperCase();
  return s;
}
