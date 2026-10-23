// HAL cho test: thoi gian, GPIO va UART gia lap tren may tinh.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
using std::min;
using String = std::string;
constexpr int LOW = 0, HIGH = 1, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
constexpr int SERIAL_8N1 = 0;
namespace hal {
inline uint32_t nowMs = 0;
inline std::array<int, 64> gpio = [] { std::array<int, 64> pins{}; pins.fill(HIGH); return pins; }();
inline uint32_t echoUs = 5 * 58;
inline std::deque<uint32_t> echoSequence;
// PC817 tren PCB dao ECHO thanh xung LOW tai GPIO18.
inline int echoActiveLevel = LOW;
inline bool hxReady = true;
inline long hxRaw = 0;
}
inline uint32_t millis() { return hal::nowMs; }
inline void delay(uint32_t ms) { hal::nowMs += ms; }
inline void delayMicroseconds(uint32_t) {}
inline void pinMode(uint8_t, int) {}
inline void digitalWrite(uint8_t pin, int level) { hal::gpio[pin] = level; }
inline int digitalRead(uint8_t pin) { return hal::gpio[pin]; }
inline uint32_t pulseIn(uint8_t pin, int level, uint32_t) {
  if (pin != 18 || level != hal::echoActiveLevel) return 0;
  if (hal::echoSequence.empty()) return hal::echoUs;
  const uint32_t duration = hal::echoSequence.front();
  hal::echoSequence.pop_front();
  return duration;
}
class HardwareSerial {
 public:
  explicit HardwareSerial(int = 0) {}
  std::deque<uint8_t> incoming;
  std::string outgoing;
  bool writable = true;
  template <typename... Args> void begin(Args...) {}
  int available() const { return static_cast<int>(incoming.size()); }
  int read() { const auto byte = incoming.front(); incoming.pop_front(); return byte; }
  size_t write(const uint8_t* data, size_t length) {
    if (!writable) return 0;
    outgoing.append(reinterpret_cast<const char*>(data), length);
    return length;
  }
  void receive(const char* line) { for (; *line; ++line) incoming.push_back(*line); }
};
inline HardwareSerial Serial;
