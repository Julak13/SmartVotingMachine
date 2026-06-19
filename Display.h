#pragma once
// =====================================================================
//  Display.h  -  Thin wrapper over the 16x2 I2C LCD.
//  This screen is the VOTER interface only - admins never see it.
// =====================================================================
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

class Display {
public:
  void begin();

  // Two centred/truncated lines (line 2 optional).
  void show(const String& l1, const String& l2 = "");

  // Render a candidate selection screen: the highlighted line gets a '>'.
  void showSelection(const String& topName, const String& botName,
                     bool cursorOnTop);

  void clear();

private:
  LiquidCrystal_I2C _lcd{LCD_ADDR, LCD_COLS, LCD_ROWS};
  String fit(const String& s);          // pad / truncate to LCD_COLS
};
