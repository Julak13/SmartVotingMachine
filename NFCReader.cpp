// =====================================================================
//  NFCReader.cpp
//
//  Why it is non-blocking (v1.1.2):
//  The PN532's card search (InListPassiveTarget) runs until a card shows
//  up - by default it never gives up. v1.0-v1.1.1 called
//  readPassiveTargetID() with a 250 ms timeout in a loop: when our wait
//  expired the chip was still searching, and the next call sent a new
//  command on top of the running one. From then on the board read the
//  reader's answers out of step and taps went unnoticed - the machine sat
//  on "Tap NFC Card". (Enrollment mostly worked because it usually runs
//  while the election is closed, when the reader is left idle, so its
//  first search starts clean.)
//
//  Now one search is armed and left running. We only ask the reader
//  "are you ready?" (a one-byte status read, exactly what the library
//  does internally) until a card has been found, read it, and only then
//  arm the next search. This is the pattern of the Adafruit library's own
//  readMifareClassicIrq example, using the I2C status byte instead of the
//  IRQ wire so it works whether or not IRQ is connected.
//
//  v1.1.0 tried a bounded search (setPassiveActivationRetries) plus
//  periodic resets instead; on the real machine that left the reader
//  "not responding", so neither is used.
// =====================================================================
#include "NFCReader.h"
#include <Wire.h>

bool NFCReader::init() {
  _armed = false;
  _nfc.begin();                          // pulses RST, wakes the chip
  uint32_t ver = _nfc.getFirmwareVersion();
  if (!ver) { _ok = false; return false; }
  _nfc.SAMConfig();                      // configure to read RFID tags
  _ok = true;
  _disarmedAt = millis() - NFC_REARM_MS; // allow arming straight away
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

// The PN532 answers a 1-byte I2C read with 0x01 once its reply is ready.
// Reading the status byte does not consume the reply.
bool NFCReader::responseReady() {
  if (Wire.requestFrom((uint8_t)PN532_I2C_ADDRESS, (size_t)1) != 1) return false;
  return Wire.read() == PN532_I2C_READY;
}

String NFCReader::takeCard() {
  // ISO14443A UIDs are 4, 7 or 10 bytes and the library copies the full
  // length it receives, so the buffer must hold the largest case.
  uint8_t uid[10] = {0};
  uint8_t uidLen  = 0;
  bool ok = _nfc.readDetectedPassiveTargetID(uid, &uidLen);
  _armed = false;                        // that search is finished
  _disarmedAt = millis();
  if (!ok || uidLen == 0 || uidLen > sizeof(uid)) return "";

  String s;
  for (uint8_t i = 0; i < uidLen; i++) {
    if (uid[i] < 0x10) s += '0';
    s += String(uid[i], HEX);
  }
  s.toUpperCase();
  return s;
}

String NFCReader::poll() {
  if (!_ok) return "";

  if (!_armed) {
    // Short pause between searches, as in the Adafruit example.
    if (millis() - _disarmedAt < NFC_REARM_MS) return "";
    _armed = true;
    // Returns true only if a card was already in the field and answered
    // within the library's ~100 ms wait; otherwise the search keeps
    // running inside the PN532 and we check on it in later calls.
    if (_nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A)) return takeCard();
    return "";
  }

  if (!responseReady()) return "";
  return takeCard();
}

void NFCReader::flush() {
  if (_ok && _armed && responseReady()) {
    takeCard();                          // discard the stale detection
    _disarmedAt = millis() - NFC_REARM_MS;
  }
}
