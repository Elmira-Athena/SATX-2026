// Gia lap I2C; test van chay driver EEPROM that trong sketch.
#pragma once
#include "Arduino.h"
class TwoWire {
 public:
  std::array<uint8_t, 32768> memory{};
  bool present = true;
  unsigned pageViolations = 0;
  std::vector<uint8_t> tx;
  uint16_t cursor = 0;
  uint8_t device = 0;
  template <typename... Args> void begin(Args...) {}
  void setClock(uint32_t) {}
  void reset() { memory.fill(0xFF); present = true; pageViolations = 0; tx.clear(); cursor = 0; }
  void beginTransmission(uint8_t address) { device = address; tx.clear(); }
  size_t write(uint8_t byte) { tx.push_back(byte); return 1; }
  size_t write(const uint8_t* data, size_t length) { tx.insert(tx.end(), data, data + length); return length; }
  uint8_t endTransmission(bool = true) {
    if (device != 0x50 || !present) return 2;
    if (tx.size() < 2) return 0;
    cursor = static_cast<uint16_t>((tx[0] << 8) | tx[1]);
    if (tx.size() > 2 && cursor / 64 != (cursor + tx.size() - 3) / 64) {
      ++pageViolations;
      return 3;
    }
    for (size_t i = 2; i < tx.size(); ++i) memory[cursor++] = tx[i];
    return 0;
  }
  uint8_t requestFrom(uint8_t, uint8_t length) { return present ? length : 0; }
  uint8_t read() { return memory[cursor++]; }
};
inline TwoWire Wire;
