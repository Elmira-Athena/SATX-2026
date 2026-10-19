// Low-current LED probe for an ESP32 removed from the PCB.
// Wiring, AFTER this sketch is uploaded: GPIO33 -> LED anode; cathode -> GND.
// GPIO33 remains INPUT throughout. Its internal pull-up limits current.
// A detached LED may glow very faintly; no visible light is inconclusive.
// Never use the normal OUTPUT/HIGH LED test without an external series resistor.
#include <Arduino.h>
#include <driver/gpio.h>

namespace {
// Gia tri cuc bo khop muc cau hinh trong SATx/SATx.ino; sketch nay chay doc lap.
constexpr uint8_t kLedRed = 33;
constexpr uint32_t kUartBaud = 115200;
constexpr uint32_t kIntervalMs = 1000;
bool ledOn = false;
uint32_t lastChangeMs = 0;

void setWeakLed(bool on) {
  pinMode(kLedRed, on ? INPUT_PULLUP : INPUT_PULLDOWN);
  gpio_io_config_t io = {};
  const esp_err_t result = gpio_get_io_config(
      static_cast<gpio_num_t>(kLedRed), &io);
  Serial.printf("LED_WEAK %s up=%lus GPIO%u in=%u out=%u pu=%u pd=%u status=%d\n",
                on ? "ON" : "OFF", static_cast<unsigned long>(millis() / 1000),
                kLedRed, io.ie, io.oe, io.pu, io.pd,
                static_cast<int>(result));
}
}  // namespace

void setup() {
  pinMode(kLedRed, INPUT_PULLDOWN);
  Serial.begin(kUartBaud);
  delay(250);
  Serial.println("BOOT: SATx WEAK LED BLINK TEST - INPUT PULLUP ONLY");
  Serial.println("Detached LED: anode to GPIO33, cathode to GND. Light may be faint.");
  ledOn = true;
  setWeakLed(ledOn);
  lastChangeMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastChangeMs >= kIntervalMs) {
    lastChangeMs = now;
    ledOn = !ledOn;
    setWeakLed(ledOn);
  }
  delay(10);
}
