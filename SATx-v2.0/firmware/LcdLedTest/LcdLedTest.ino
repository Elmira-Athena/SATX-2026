// Temporary hardware diagnostic: LCD + status LEDs only.
// Uses the same pin configuration as SATx; no sensors, WiFi, EEPROM or NVS writes.
// Red for 2 seconds, green for 2 seconds, then both stay on.
// This sketch replaces the SATx application on the ESP32 while testing.
#include <Arduino.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

namespace {
// Gia tri cuc bo khop muc cau hinh trong SATx/SATx.ino; sketch nay chay doc lap.
constexpr uint8_t kLedRed = 33;
constexpr uint8_t kLedGreen = 32;
constexpr uint8_t kI2cSda = 21;
constexpr uint8_t kI2cScl = 22;
constexpr uint32_t kClockHz = 100000;
constexpr uint32_t kUartBaud = 115200;
constexpr uint8_t kCols = 16;
constexpr uint8_t kRows = 2;

hd44780_I2Cexp lcd;
bool lcdReady = false;
uint32_t testStartedMs = 0;
uint32_t lastReportMs = 0;
uint8_t previousPhase = 255;
constexpr uint32_t kSingleLedMs = 2000;

void setLeds(bool red, bool green) {
  digitalWrite(kLedRed, red ? HIGH : LOW);
  digitalWrite(kLedGreen, green ? HIGH : LOW);
}

void scanI2c() {
  uint8_t found = 0;
  Serial.println("I2C_SCAN_BEGIN");
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t result = Wire.endTransmission();
    if (result == 0) {
      Serial.printf("I2C_FOUND=0x%02X\n", address);
      ++found;
    } else if (result != 2) {
      Serial.printf("I2C_ERROR addr=0x%02X code=%u\n", address, result);
    }
  }
  Serial.printf("I2C_SCAN_END count=%u\n", found);
}
}  // namespace

void setup() {
  // LEDs work independently of the LCD initialization result.
  pinMode(kLedRed, OUTPUT);
  pinMode(kLedGreen, OUTPUT);
  setLeds(true, false);

  Serial.begin(kUartBaud);
  delay(250);
  Serial.println("BOOT: SATx LCD + LED TEST");
  Serial.printf("RED=GPIO%u GREEN=GPIO%u SDA=GPIO%u SCL=GPIO%u I2C=%luHz\n",
                kLedRed, kLedGreen,
                kI2cSda, kI2cScl,
                static_cast<unsigned long>(kClockHz));

  const bool busReady = Wire.begin(kI2cSda, kI2cScl, kClockHz);
  Wire.setTimeOut(25);
  if (busReady) {
    scanI2c();
    const int status = lcd.begin(kCols, kRows);
    lcdReady = (status == 0);
    Serial.printf("LCD_INIT=%d (%s)\n", status, lcdReady ? "OK" : "FAILED");
    if (lcdReady) {
      lcd.backlight();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SATx LCD TEST");
    }
  } else {
    Serial.println("I2C_BEGIN_FAILED");
  }
  testStartedMs = millis();
}

void loop() {
  const uint32_t now = millis();
  const uint32_t elapsed = now - testStartedMs;
  const uint8_t phase = elapsed < kSingleLedMs ? 0
                       : elapsed < 2 * kSingleLedMs ? 1 : 2;

  if (phase != previousPhase) {
    previousPhase = phase;
    setLeds(phase != 1, phase != 0);
    const char* label = phase == 0 ? "LED: RED        "
                        : phase == 1 ? "LED: GREEN      "
                                     : "LEDS: BOTH ON   ";
    Serial.println(label);
    if (lcdReady) {
      lcd.setCursor(0, 1);
      lcd.print(label);
    }
  }

  if (now - lastReportMs >= 2000) {
    lastReportMs = now;
    Serial.printf("ALIVE up=%lus lcd=%s phase=%u\n",
                  static_cast<unsigned long>(now / 1000),
                  lcdReady ? "OK" : "FAILED", phase);
  }
  delay(10);
}
