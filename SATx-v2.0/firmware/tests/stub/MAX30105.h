#pragma once
#include "Wire.h"
class MAX30105 {
 public:
  std::deque<std::pair<uint32_t, uint32_t>> samples;
  bool begin(TwoWire&, uint32_t, uint8_t) { return true; }
  template <typename... Args> void setup(Args...) {}
  void check() {}
  bool available() const { return !samples.empty(); }
  uint32_t getFIFORed() const { return samples.front().first; }
  uint32_t getFIFOIR() const { return samples.front().second; }
  void nextSample() { samples.pop_front(); }
};
