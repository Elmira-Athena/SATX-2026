#pragma once
#include "Arduino.h"
constexpr int WIFI_STA = 1, WL_CONNECTED = 3;
class WiFiClass {
 public:
  bool sleeping = true;
  void mode(int) {}
  void setAutoReconnect(bool) {}
  void begin(const char*, const char*) {}
  void setSleep(bool enabled) { sleeping = enabled; }
  int status() const { return WL_CONNECTED; }
  struct Address { String toString() const { return "192.168.1.63"; } };
  Address localIP() const { return {}; }
};
inline WiFiClass WiFi;
