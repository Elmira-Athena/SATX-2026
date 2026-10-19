#pragma once
class MDNSClass {
 public:
  void end() {}
  bool begin(const char*) { return true; }
  void addService(const char*, const char*, unsigned) {}
};
inline MDNSClass MDNS;
