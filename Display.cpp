// =====================================================================
//  Display.cpp
// =====================================================================
#include "Display.h"

// The idle loop asks for the same screen several times a second. Each LCD
// line costs ~16 I2C transfers on the bus the NFC reader also uses, so only
// changed lines are written. A periodic full rewrite still repairs the
// screen if electrical noise ever garbles it.
static const uint32_t LCD_REFRESH_MS = 5000;

void Display::begin() {
  _lcd.begin();         // NOTE: this LiquidCrystal_I2C fork exposes begin(),
  _lcd.backlight();     // not init(). Do not change to init() - won't compile.
  clear();
}

void Display::clear() {
  _lcd.clear();
  _line1 = ""; _line2 = "";
  _lastFull = millis();
}

String Display::fit(const String& s) {
  String out = s;
  if (out.length() > LCD_COLS) out = out.substring(0, LCD_COLS);
  while (out.length() < LCD_COLS) out += ' ';   // clear leftover chars
  return out;
}

void Display::writeLines(const String& a, const String& b) {
  bool full = (millis() - _lastFull) >= LCD_REFRESH_MS;
  if (full || a != _line1) { _lcd.setCursor(0, 0); _lcd.print(a); _line1 = a; }
  if (full || b != _line2) { _lcd.setCursor(0, 1); _lcd.print(b); _line2 = b; }
  if (full) _lastFull = millis();
}

void Display::show(const String& l1, const String& l2) {
  writeLines(fit(l1), fit(l2));
}

void Display::showSelection(const String& topName, const String& botName,
                            bool cursorOnTop) {
  String t = (cursorOnTop ? ">" : " ") + topName;
  String b = (cursorOnTop ? " " : ">") + botName;
  writeLines(fit(t), fit(b));
}
