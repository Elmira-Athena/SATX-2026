#pragma once
#include "Arduino.h"
class hd44780_I2Cexp {
 public:
  int begin(uint8_t, uint8_t) { return 0; }
  void backlight() {}
  void clear() {}
  void setCursor(uint8_t, uint8_t) {}
  void print(const char*) {}
};
