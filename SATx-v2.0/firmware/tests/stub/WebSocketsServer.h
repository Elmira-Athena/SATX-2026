#pragma once
#include "Arduino.h"
enum WStype_t { WStype_CONNECTED, WStype_DISCONNECTED, WStype_TEXT };
class WebSocketsServer {
 public:
  explicit WebSocketsServer(unsigned) {}
  std::string outgoing;
  bool writable = true;
  std::function<void(uint8_t, WStype_t, uint8_t*, size_t)> callback;
  void begin() {}
  template <typename Callback> void onEvent(Callback cb) { callback = cb; }
  void loop() {}
  bool broadcastTXT(const uint8_t* data, size_t length) {
    if (!writable) return false;
    outgoing.append(reinterpret_cast<const char*>(data), length);
    return true;
  }
  void receive(WStype_t type, const char* payload = "") {
    if (callback) callback(0, type, reinterpret_cast<uint8_t*>(const_cast<char*>(payload)), strlen(payload));
  }
};
