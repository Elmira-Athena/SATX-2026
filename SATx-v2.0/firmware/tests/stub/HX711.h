#pragma once
#include "Arduino.h"
class HX711 {
 public:
  void begin(uint8_t, uint8_t) {}
  bool is_ready() const { return hal::hxReady; }
  long read() const { return hal::hxRaw; }
};
