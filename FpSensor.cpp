// =====================================================================
//  FpSensor.cpp
// =====================================================================
#include "FpSensor.h"

bool FpSensor::begin() {
  // Serial2 is started in the .ino with the correct pins before this call.
  _f.begin(FP_BAUD);
  delay(50);
  _ok = _f.verifyPassword();
  if (_ok) {
    _f.getTemplateCount();
    Serial.printf("[FP] sensor OK, %u templates stored\n", _f.templateCount);
  } else {
    Serial.println(F("[FP] sensor NOT found - check wiring/baud"));
  }
  return _ok;
}

uint16_t FpSensor::templateCount() {
  if (!_ok) return 0;
  _f.getTemplateCount();
  return _f.templateCount;
}

int FpSensor::search() {
  if (!_ok) return -1;

  int p = _f.getImage();
  if (p == FINGERPRINT_NOFINGER) return 0;
  if (p != FINGERPRINT_OK)       return -1;

  if (_f.image2Tz() != FINGERPRINT_OK)        return -1;
  if (_f.fingerFastSearch() != FINGERPRINT_OK) return -1;

  _conf = _f.confidence;
  if (_conf < FP_MIN_CONFIDENCE) return -1;
  return _f.fingerID;
}

int FpSensor::captureImage() {
  if (!_ok) return FINGERPRINT_PACKETRECIEVEERR;
  return _f.getImage();
}

bool FpSensor::toTemplate(uint8_t slot) {
  return _ok && _f.image2Tz(slot) == FINGERPRINT_OK;
}

bool FpSensor::createModel() {
  return _ok && _f.createModel() == FINGERPRINT_OK;
}

bool FpSensor::store(uint16_t id) {
  return _ok && _f.storeModel(id) == FINGERPRINT_OK;
}

bool FpSensor::remove(uint16_t id) {
  return _ok && _f.deleteModel(id) == FINGERPRINT_OK;
}

bool FpSensor::waitFingerRemoved(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (_f.getImage() == FINGERPRINT_NOFINGER) return true;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  return false;
}
