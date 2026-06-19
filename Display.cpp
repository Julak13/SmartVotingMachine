// =====================================================================
//  Display.cpp
// =====================================================================
#include "Display.h"

void Display::begin() {
  _lcd.begin();         // NOTE: this LiquidCrystal_I2C fork exposes begin(),
  _lcd.backlight();     // not init(). Do not change to init() - won't compile.
  clear();
}

void Display::clear() { _lcd.clear(); }

String Display::fit(const String& s) {
  String out = s;
  if (out.length() > LCD_COLS) out = out.substring(0, LCD_COLS);
  while (out.length() < LCD_COLS) out += ' ';   // clear leftover chars
  return out;
}

void Display::show(const String& l1, const String& l2) {
  _lcd.setCursor(0, 0); _lcd.print(fit(l1));
  _lcd.setCursor(0, 1); _lcd.print(fit(l2));
}

void Display::showSelection(const String& topName, const String& botName,
                            bool cursorOnTop) {
  String t = (cursorOnTop ? ">" : " ") + topName;
  String b = (cursorOnTop ? " " : ">") + botName;
  _lcd.setCursor(0, 0); _lcd.print(fit(t));
  _lcd.setCursor(0, 1); _lcd.print(fit(b));
}
