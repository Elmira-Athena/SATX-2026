#pragma once
#include "Arduino.h"
class Preferences {
 public:
  std::map<std::string, float> floats;
  std::map<std::string, std::string> strings;
  std::string nameSpace;
  bool begin(const char* name, bool) { nameSpace = name; return true; }
  float getFloat(const char* key, float fallback) const {
    const auto found = floats.find(key);
    return found == floats.end() ? fallback : found->second;
  }
  size_t getString(const char* key, char* out, size_t size) const {
    const auto found = strings.find(key);
    return strlcpy(out, found == strings.end() ? "" : found->second.c_str(), size);
  }
  size_t putFloat(const char* key, float value) { floats[key] = value; return sizeof(value); }
  size_t putString(const char* key, const char* value) { strings[key] = value; return strlen(value); }
};
