// SATx - Tram do suc khoe ESP32 DevKit V1 (WROOM-32).
// Toan bo firmware nam trong file nay, khong can thu muc src/.
// Cach doc: cau hinh -> du lieu -> setup() -> loop() -> ham cua tung thiet bi.
// Chu trinh: khoi dong -> san sang -> nhip tim/SpO2 -> chieu cao -> can nang -> ket qua.
// Nut RESET luon dua ve san sang; loi cam bien van giu cac chi so do duoc.
// Thu vien: SparkFun MAX3010x, HX711 (bogde), hd44780, ArduinoJson 7, WebSockets.
// Chi sua chan/nguong ben duoi sau khi xem docs/cau-hinh-chan-io.md va hieu-chuan.md.

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <math.h>
#include <string.h>

// ===== 1. CAU HINH: CHAN, NGUONG, THOI GIAN =====
namespace cfg {

// -----------------------------------------------------------------------------
//  Che do bien dich
// -----------------------------------------------------------------------------

// Bat = 1 de firmware sinh so do gia thay vi doc cam bien that.
// Dung de kiem thu end-to-end giao thuc + Web UI khi chua co cam bien.
#ifndef SATX_SIMULATE_SENSORS
#define SATX_SIMULATE_SENSORS 0
#endif

// Chon cong UART noi voi CH340 cua giao dien.
//   0 = UART0 (GPIO1/GPIO3) — dung nhu schematic ve.
//   2 = UART2 (GPIO17/GPIO16) — khuyen nghi, tranh xung dot voi CH340 onboard.
// Xem canh bao HW-2 trong docs/cau-hinh-chan-io.md
#ifndef SATX_LINK_UART_NUM
#define SATX_LINK_UART_NUM 0
#endif

// -----------------------------------------------------------------------------
//  Chan IO
// -----------------------------------------------------------------------------
namespace pin {

// I2C dung chung: LCD + MAX30102 + EEPROM. Xem HW-6: PDF noi nham SD1/SD0.
// Phai dau vao GPIO21/22 that; R4/R5 trong PDF la pull-up nut nhan, khong phai I2C.
constexpr uint8_t kI2cSda = 21;   // net SDA_LCD / SDA_M3 / SDA_EE
constexpr uint8_t kI2cScl = 22;   // net SCL_LCD / SCL_M3 / SCL_EE

// HC-SR04 do chieu cao
constexpr uint8_t kUltrasonicTrig = 5;    // net TRIG
constexpr uint8_t kUltrasonicEcho = 18;   // net ECHO, qua PC817 + R 220R (HW-1)

// HX711 doc loadcell
constexpr uint8_t kLoadCellData  = 34;    // net DAT — GPIO34 chi vao, dung cho DOUT
constexpr uint8_t kLoadCellClock = 19;    // net CLK

// Den bao trang thai (R 330R)
constexpr uint8_t kLedRed   = 33;   // sang khi dang do
constexpr uint8_t kLedGreen = 32;   // sang khi do xong

// Nut nhan (H3/H4), noi xuong GND -> dung pull-up noi bo, muc tich cuc LOW
constexpr uint8_t kButtonStart = 26;
constexpr uint8_t kButtonReset = 25;

// UART2 khi SATX_LINK_UART_NUM == 2
constexpr uint8_t kUart2Tx = 17;
constexpr uint8_t kUart2Rx = 16;

}  // namespace pin

// -----------------------------------------------------------------------------
//  Bus I2C
// -----------------------------------------------------------------------------
namespace i2c {

constexpr uint32_t kClockHz = 100000;   // PCF8574 chi ho tro toi da 100 kHz
constexpr uint8_t  kAddrLcd      = 0x27;  // PCF8574; mot so module la 0x3F
constexpr uint8_t  kAddrPulseOx  = 0x57;  // MAX30102 (co dinh)
constexpr uint8_t  kAddrEeprom   = 0x50;  // AT24C256, A0=A1=A2=GND

}  // namespace i2c

// -----------------------------------------------------------------------------
//  Man hinh LCD
// -----------------------------------------------------------------------------
namespace lcd {

constexpr uint8_t kCols = 16;
constexpr uint8_t kRows = 2;

}  // namespace lcd

// -----------------------------------------------------------------------------
//  HC-SR04 — do chieu cao
// -----------------------------------------------------------------------------
namespace height {

// Muc logic cua chan ECHO khi dang co xung vong ve.
// PC817 mac kieu cuc gop chung + pull-up SE DAO MUC -> khi do doi thanh LOW.
// Bat buoc xac minh bang bai test T-HCSR04 truoc khi tin so do. (HW-1)
constexpr int  kEchoActiveLevel = 1;      // 1 = HIGH, 0 = LOW

constexpr uint32_t kTrigPulseUs    = 10;      // do rong xung kich theo datasheet
constexpr uint32_t kEchoTimeoutUs  = 30000;   // 30 ms ~ 5 m, du xa
constexpr uint8_t  kSampleCount    = 5;       // lay 5 mau roi loc trung vi
constexpr uint32_t kSampleGapMs    = 60;      // >= 60 ms de tranh vong doi am

// Nguong hop le cua khoang cach tho (cm) — ngoai khoang nay coi la mau hong
constexpr float kMinDistanceCm = 2.0f;
constexpr float kMaxDistanceCm = 400.0f;

// Nguong hop le cua chieu cao nguoi (cm) sau khi quy doi
constexpr float kMinHeightCm = 50.0f;
constexpr float kMaxHeightCm = 250.0f;

// Do cao lap cam bien so voi mat san (cm). Gia tri mac dinh khi chua hieu chuan;
// gia tri that duoc luu trong NVS va chinh tu trang Hieu chuan cua Web UI.
constexpr float kDefaultMountHeightCm = 200.0f;

// Toc do am thanh: quang duong = thoi_gian_us / 58.0 (cm), da tinh ca duong ve
constexpr float kUsPerCm = 58.0f;

}  // namespace height

// -----------------------------------------------------------------------------
//  HX711 — do can nang
// -----------------------------------------------------------------------------
namespace weight {

constexpr uint8_t kSampleCount = 7;    // lay 7 mau, bo 1 max + 1 min, trung binh 5
constexpr uint8_t kTrimCount   = 1;    // so mau bi cat o moi dau
constexpr uint32_t kReadTimeoutMs = 1000;

// He so quy doi mac dinh (raw tren mot kg) khi chua hieu chuan.
// Gia tri that nam trong NVS, dat qua {"t":"cal","target":"weight"}.
constexpr float kDefaultScale = 2852.35f;

// Nguong hop le cua can nang (kg)
constexpr float kMinWeightKg = 2.0f;
constexpr float kMaxWeightKg = 250.0f;

}  // namespace weight

// -----------------------------------------------------------------------------
//  MAX30102 — nhip tim va SpO2
// -----------------------------------------------------------------------------
namespace pulseox {

constexpr uint8_t  kLedBrightness = 0x1F;   // ~6.4 mA, du cho ngon tay
constexpr uint8_t  kSampleAverage = 4;
constexpr uint8_t  kLedMode       = 2;      // 2 = do + hong ngoai (can cho SpO2)
constexpr uint16_t kSampleRateHz  = 100;
constexpr uint16_t kPulseWidthUs  = 411;
constexpr uint16_t kAdcRange      = 4096;

// Phat hien ngon tay — nguong tren bien do hong ngoai, co tre chong rung
constexpr uint32_t kFingerOnThreshold  = 45000;
constexpr uint32_t kFingerOffThreshold = 35000;
constexpr uint32_t kFingerDebounceMs   = 300;

constexpr uint32_t kWarmupMs   = 2000;    // bo qua 2 s dau cho DC filter on dinh
constexpr uint32_t kMeasureMs  = 10000;   // cua so dem nhip, giong ban PIC

// Nguong hop le cua ket qua
constexpr uint8_t kMinHeartRate = 35;
constexpr uint8_t kMaxHeartRate = 220;
constexpr uint8_t kMinSpo2      = 70;
constexpr uint8_t kMaxSpo2      = 100;

}  // namespace pulseox

// -----------------------------------------------------------------------------
//  Nut nhan
// -----------------------------------------------------------------------------
namespace button {

constexpr uint32_t kDebounceMs   = 30;
constexpr uint32_t kRepeatLockMs = 400;   // chan bam nhanh lien tiep

}  // namespace button

// -----------------------------------------------------------------------------
//  Den bao
// -----------------------------------------------------------------------------
namespace led {

constexpr uint32_t kBlinkPeriodMs = 500;   // chu ky nhay khi dang do

}  // namespace led

// -----------------------------------------------------------------------------
//  Ket noi va giao thuc
// -----------------------------------------------------------------------------
namespace link {

constexpr uint32_t kUartBaud = 115200;

// Giao dien gui {"t":"ping"} moi kHeartbeatMs; qua kUartTimeoutMs khong thay
// ping nao thi coi nhu dut UART.
constexpr uint32_t kHeartbeatMs   = 1000;
constexpr uint32_t kUartTimeoutMs = 3000;
constexpr uint32_t kWifiTimeoutMs = 5000;

constexpr uint16_t kWebSocketPort = 81;
constexpr char     kMdnsHostname[] = "satx";   // -> satx.local

// Do dai toi da mot dong JSON
constexpr size_t kMaxLineLen = 512;

}  // namespace link

// -----------------------------------------------------------------------------
//  EEPROM ngoai (AT24C256)
// -----------------------------------------------------------------------------
namespace eeprom {

constexpr uint32_t kCapacityBytes = 32768;   // AT24C32 -> 4096
constexpr uint16_t kPageSizeBytes = 64;      // AT24C32 -> 32
constexpr uint32_t kWriteCycleMs  = 5;       // thoi gian ghi mot trang
constexpr uint8_t  kAddrBytes     = 2;       // AT24C32/256 dung dia chi 2 byte

constexpr uint16_t kHeaderSize    = 16;
constexpr uint16_t kRecordSize    = 16;
constexpr uint16_t kRecordAreaStart = kHeaderSize;
constexpr uint16_t kMaxRecords =
    static_cast<uint16_t>((kCapacityBytes - kRecordAreaStart) / kRecordSize);

constexpr uint32_t kMagic   = 0x53415458;   // "SATX"
constexpr uint8_t  kVersion = 1;

}  // namespace eeprom

// -----------------------------------------------------------------------------
//  Chu trinh do
// -----------------------------------------------------------------------------
namespace flow {

// Moc thoi gian toi da cho tung buoc; qua han thi bao loi thay vi treo.
constexpr uint32_t kStepPulseOxTimeoutMs = 60000;   // co the PAUSE khi roi tay
constexpr uint32_t kStepHeightTimeoutMs  = 3000;
constexpr uint32_t kStepWeightTimeoutMs  = 3000;

constexpr uint32_t kProgressIntervalMs = 250;   // nhip phat {"t":"state"}
constexpr uint32_t kErrorHoldMs        = 3000;  // giu man hinh loi truoc khi ve IDLE

}  // namespace flow

}  // namespace cfg

#if !SATX_SIMULATE_SENSORS
#include <MAX30105.h>
#include <spo2_algorithm.h>
#endif

// ===== 2. KIEU DU LIEU =====
enum class Status : uint8_t {
  kOk = 0,
  kTimeout,        // thiet bi khong phan hoi trong han
  kNotPresent,     // khong tim thay tren bus I2C
  kOutOfRange,     // doc duoc nhung gia tri nam ngoai nguong hop le
  kNoisy,          // qua nhieu mau hong, khong du du lieu tin cay
  kNotReady,       // chua den luc, thu lai sau
  kNotCalibrated,  // thieu he so hieu chuan
  kCrcError,       // du lieu luu tru bi hong
  kFull,           // het cho luu
  kEmpty,          // khong co du lieu de doc
  kIoError,        // loi bus/ghi doc chung
  kInvalidArg,
};

// Ten ma loi dang chuoi, dung cho truong "code" cua ban tin {"t":"error"}.
inline const char* toString(Status s) {
  switch (s) {
    case Status::kOk:             return "OK";
    case Status::kTimeout:        return "TIMEOUT";
    case Status::kNotPresent:     return "NOT_PRESENT";
    case Status::kOutOfRange:     return "OUT_OF_RANGE";
    case Status::kNoisy:          return "NOISY";
    case Status::kNotReady:       return "NOT_READY";
    case Status::kNotCalibrated:  return "NOT_CALIBRATED";
    case Status::kCrcError:       return "CRC_ERROR";
    case Status::kFull:           return "FULL";
    case Status::kEmpty:          return "EMPTY";
    case Status::kIoError:        return "IO_ERROR";
    case Status::kInvalidArg:     return "INVALID_ARG";
  }
  return "UNKNOWN";
}

inline bool isOk(Status s) { return s == Status::kOk; }

// Co bao tinh hop le cua tung truong. Mot lan do van duoc luu du mot vai chi so
// khong doc duoc — giao dien se hien "--" cho dung truong do thay vi so 0 gia.
namespace flag {
constexpr uint8_t kTimeValid   = 1 << 0;
constexpr uint8_t kWeightValid = 1 << 1;
constexpr uint8_t kHeightValid = 1 << 2;
constexpr uint8_t kHeartValid  = 1 << 3;
constexpr uint8_t kSpo2Valid   = 1 << 4;
}  // namespace flag

struct MeasurementRecord {
  uint32_t epochSec   = 0;    // 0 khi chua tung dong bo gio voi PC
  float    weightKg   = 0.0f;
  float    heightCm   = 0.0f;
  uint8_t  heartRate  = 0;    // bpm
  uint8_t  spo2       = 0;    // %
  float    bmi        = 0.0f; // tinh tren ESP32 de LCD va web luon khop
  uint8_t  flags      = 0;

  bool has(uint8_t f) const { return (flags & f) != 0; }
  void set(uint8_t f, bool on) { on ? (flags |= f) : (flags &= static_cast<uint8_t>(~f)); }

  // Du lieu do coi la day du khi ca bon chi so deu hop le.
  bool isComplete() const {
    return has(flag::kWeightValid) && has(flag::kHeightValid) &&
           has(flag::kHeartValid) && has(flag::kSpo2Valid);
  }

  void clear() { *this = MeasurementRecord{}; }

  // BMI = kg / m^2. Chi tinh khi ca can nang va chieu cao deu hop le.
  void computeBmi() {
    if (!has(flag::kWeightValid) || !has(flag::kHeightValid) || heightCm <= 0.0f) {
      bmi = 0.0f;
      return;
    }
    const float meters = heightCm / 100.0f;
    bmi = weightKg / (meters * meters);
  }
};

// =============================================================================
//  Ban tin JSON-lines dung chung cho ca UART va WebSocket.
//
//  Mot ban tin = mot dong JSON + '\n'. Ben nhan BO QUA moi dong khong parse
//  duoc: cong UART0 con dung chung voi log bootloader cua ESP32, va nhieu duong
//  truyen co the sinh rac.
//
//  Dac ta day du: docs/giao-thuc.md
// =============================================================================

// Lenh tu giao dien gui xuong.
enum class CommandType : uint8_t {
  kUnknown,
  kPing,     // nhip tim ket noi
  kSync,     // dong bo dong ho: {"epoch":...}
  kMeasure,  // bat dau do
  kReset,    // dua ve trang thai san sang
  kFlush,    // yeu cau day het ban ghi trong EEPROM
  kCalibrate,
  kConfig,
  kEraseStore,
};

enum class CalibrationTarget : uint8_t { kNone, kWeightTare, kWeightKnown, kHeightMount };

struct Command {
  CommandType       type   = CommandType::kUnknown;
  CalibrationTarget target = CalibrationTarget::kNone;
  uint32_t          epoch  = 0;
  float             value  = 0.0f;   // known_kg hoac mount_cm
  char              ssid[33]     = {0};
  char              password[65] = {0};
  bool              physicalErase = false;
};

// Nguon goc cua mot ban ghi ket qua.
enum class RecordSource : uint8_t { kLive, kEeprom };

enum class PulsePhase : uint8_t {
  kIdle,      // chua bat dau
  kWarmup,    // dang cho bo loc DC on dinh
  kSampling,  // dang dem nhip
  kPaused,    // mat ngon tay — dong ho tam dung
  kFinished,  // du du lieu, doc ket qua bang result()
};

enum class LedPattern : uint8_t {
  kIdle,       // ca hai tat
  kMeasuring,  // do nhay, xanh tat
  kDone,       // xanh sang, do tat
  kError,      // do sang lien tuc, xanh tat
};

enum class AppState : uint8_t {
  kBoot,
  kSelfTest,
  kIdle,
  kMeasurePpg,
  kMeasureHeight,
  kMeasureWeight,
  kDone,
  kError,
};

enum class LinkState : uint8_t { kNoLink, kUartActive, kWifiActive };

// Hai nut dung cung cach chong doi, nhung moi nut nho trang thai rieng.
struct Button {
  uint8_t pin;
  bool stable = false;
  bool lastRaw = false;
  uint32_t lastChangeMs = 0;
  uint32_t lastAcceptMs = 0;
};

// UART va WiFi moi kenh co mot bo dem dong, khong dung chung du lieu dang nhan.
struct LineBuffer {
  char buffer[cfg::link::kMaxLineLen] = {0};
  size_t length = 0;
  bool overflow = false;
};

// ===== 3. BIEN TRANG THAI =====
// Thiet bi va du lieu duoc giu lai giua cac lan loop().
hd44780_I2Cexp g_lcd;
HX711 g_hx711;
#if !SATX_SIMULATE_SENSORS
MAX30105 g_sensor;
#endif
Preferences g_prefs;
WebSocketsServer g_server(cfg::link::kWebSocketPort);
#if SATX_LINK_UART_NUM == 2
HardwareSerial linkPort(2);
#else
HardwareSerial& linkPort = Serial;
#endif

float settingsWeightOffset = 0.0f;
float settingsWeightScale = 0.0f;
float settingsMountHeightCm = 0.0f;
char settingsSsid[33] = {0};
char settingsPassword[65] = {0};
uint32_t epochAtSync = 0;
uint32_t millisAtSync = 0;
bool clockValid = false;

bool eepromPresent = false;
bool storeAvailable = false;
uint16_t storeCount = 0;
uint16_t storeHead = 0;
constexpr uint16_t kChunkBytes = 32; // du cho moi bo dem Wire, ke ca core cu
constexpr uint16_t kRecCrcCover = 14;

bool displayPresent = false;
char displayShadow[2][17] = {{0}, {0}};
float heightMountHeightCm = 0.0f;
float heightLastDistanceCm = 0.0f;
float weightOffset = 0.0f;
float weightScale = 0.0f;

constexpr uint8_t kBufferLen = 100; // 4 giay @ 25 mau/giay hieu dung
constexpr uint8_t kShiftLen = 25;
constexpr uint8_t kMaxEstimates = 16;
bool pulsePresent = false;
PulsePhase pulsePhase = PulsePhase::kIdle;
bool pulseFingerPresent = false;
uint32_t pulseIrBuffer[kBufferLen] = {0};
uint32_t pulseRedBuffer[kBufferLen] = {0};
uint8_t pulseFilled = 0;
uint32_t pulsePhaseStartMs = 0;
uint32_t pulseSampledMs = 0;
uint32_t pulseFingerChangeMs = 0;
uint8_t pulseHrEstimates[kMaxEstimates] = {0};
uint8_t pulseSpo2Estimates[kMaxEstimates] = {0};
uint8_t pulseEstimateCount = 0;

Button startButton{cfg::pin::kButtonStart};
Button resetButton{cfg::pin::kButtonReset};
LedPattern ledPattern = LedPattern::kIdle;
uint32_t ledBlinkRefMs = 0;

LineBuffer uartBuffer;
LineBuffer wifiBuffer;
uint32_t uartLastPeerMs = 0;
bool uartSeenPeer = false;
uint32_t wifiLastPeerMs = 0;
bool wifiSeenPeer = false;
char wifiIp[16] = "0.0.0.0";
uint8_t wifiClientCount = 0;
bool wifiActive = false;
bool wifiStarted = false;
LinkState linkState = LinkState::kNoLink;
bool linkMeasuring = false;
uint32_t linkNowMs = 0;

bool devLcd = false;
bool devPulseOx = false;
bool devEeprom = false;
AppState state = AppState::kBoot;
Status lastError = Status::kOk;
MeasurementRecord record;
uint32_t stateEnteredMs = 0;
uint32_t lastProgressMs = 0;
bool flushing = false;
uint16_t flushIndex = 0;
uint32_t lastFlushMs = 0;
uint32_t lastPongMs = 0;
char g_outBuffer[cfg::link::kMaxLineLen];
constexpr uint32_t kFlushIntervalMs = 40;

struct HeaderBytes {
  static constexpr uint16_t kMagicOff   = 0;
  static constexpr uint16_t kVersionOff = 4;
  static constexpr uint16_t kCountOff   = 6;
  static constexpr uint16_t kHeadOff    = 8;
  static constexpr uint16_t kCrcOff     = 10;
  static constexpr uint16_t kCrcCover   = 10;
};

// Khai bao ham de Arduino/C++ nhan ra cac kieu du lieu o tren.
const char* stateName();
const char* stepName();
const char* linkActiveName();
void setup();
void loop();
void enterIdle(uint32_t nowMs);
void enterMeasuring(uint32_t nowMs);
void enterDone(uint32_t nowMs);
void enterError(Status status, uint32_t nowMs);
void stepPulseOx(uint32_t nowMs);
void stepHeight(uint32_t nowMs);
void stepWeight(uint32_t nowMs);
void handleCommand(const Command& command);
void handleCalibration(const Command& command);
void emitState(uint32_t nowMs);
void emitStateNow();
void emitError(Status status);
void emitInfo();
void emitSelfTest();
void publishRecord(const MeasurementRecord& record, RecordSource source);
void serviceStore(uint32_t nowMs);
void serviceHeartbeat(uint32_t nowMs);
uint8_t progressPercent(uint32_t nowMs);
uint16_t etaSeconds(uint32_t nowMs);
void buttonBegin(Button& button);
bool buttonUpdate(Button& button, uint32_t nowMs);
void ledsBegin();
void ledsSet(LedPattern pattern);
void ledsUpdate(uint32_t nowMs);
void ledsWrite(bool red, bool green);
Status displayBegin();
void displayWriteLine(uint8_t row, const char* text);
void displayShowBoot();
void displayShowIdle();
void displayShowMeasuring(const char* stepLabel, uint8_t percent);
void displayShowPaused();
void displayShowResult(const MeasurementRecord& r);
void displayShowError(const char* code);
void sortAscending(float* values, uint8_t count);
void heightBegin();
Status heightReadDistanceOnce(float& distanceCmOut);
Status heightMeasure(float& heightCmOut);
void weightBegin();
void weightSetCalibration(float offset, float scale);
Status weightReadFilteredRaw(float& rawOut);
Status weightMeasure(float& weightKgOut);
Status weightTare();
Status weightCalibrateWith(float knownKg);
uint8_t medianOf(const uint8_t* values, uint8_t count);
Status pulseBegin();
void pulseStart(uint32_t nowMs);
void pulseResetWindow(uint32_t nowMs);
void pulseDetectFinger(uint32_t irValue, uint32_t nowMs);
void pulsePullFifoSamples();
void pulseRunAlgorithmIfReady();
PulsePhase pulseUpdate(uint32_t nowMs);
uint8_t pulseProgressPercent(uint32_t nowMs);
Status pulseResult(uint8_t& heartRateOut, uint8_t& spo2Out);
bool lineFeed(LineBuffer& line, char c);
void lineReset(LineBuffer& line);
void uartBegin();
bool uartSend(const char* line, size_t length);
bool uartIsPeerAlive(uint32_t nowMs);
void wifiBegin();
void wifiSetActive(bool active);
void wifiUpdate();
bool wifiSend(const char* line, size_t length);
bool wifiIsPeerAlive(uint32_t nowMs);
void handleLine(bool fromUart, const char* line, uint32_t nowMs);
void onWebSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length);
void linkBegin();
void linkSwitchTo(LinkState next);
void linkUpdate(uint32_t nowMs);
bool linkSend(const char* line);
CommandType typeFromString(const char* t);
bool parseCommand(const char* line, Command& commandOut);
size_t encodePong(char* out, size_t outSize, const char* linkName, uint32_t uptimeSec, uint16_t pendingRecords);
size_t encodeState(char* out, size_t outSize, const char* stateName, const char* stepName, uint8_t percent, uint16_t etaSec);
size_t encodeResult(char* out, size_t outSize, const MeasurementRecord& r, RecordSource source);
size_t encodeError(char* out, size_t outSize, const char* code, const char* message);
size_t encodeLog(char* out, size_t outSize, const char* level, const char* message);
size_t encodeInfo(char* out, size_t outSize, const char* linkName, const char* ip, uint16_t pendingRecords, float mountHeightCm, float weightScale);
size_t encodeSelfTest(char* out, size_t outSize, bool lcd, bool pulseOx, bool eeprom);
void clockSync(uint32_t epochSec, uint32_t nowMs);
uint32_t clockNowEpoch(uint32_t nowMs);
void settingsBegin();
void settingsSetWeightCalibration(float offset, float scale);
void settingsSetMountHeightCm(float cm);
void settingsSetWifiCredentials(const char* ssid, const char* password);
Status eepromBegin();
Status eepromWaitWriteComplete();
Status eepromRead(uint16_t address, uint8_t* buffer, uint16_t length);
Status eepromWritePage(uint16_t address, const uint8_t* data, uint16_t length);
Status eepromWrite(uint16_t address, const uint8_t* data, uint16_t length);
Status eepromErasePhysical(uint16_t fromAddress, uint16_t length);
void put16(uint8_t* p, uint16_t v);
void put32(uint8_t* p, uint32_t v);
uint16_t get16(const uint8_t* p);
uint32_t get32(const uint8_t* p);
uint16_t toFixed(float value, float factor);
void serialize(const MeasurementRecord& r, uint8_t* out);
Status deserialize(const uint8_t* in, MeasurementRecord& r);
uint16_t crc16Ccitt(const uint8_t* data, uint16_t length);
uint16_t storeSlotAddress(uint16_t slot);
Status storeBegin();
Status storeLoadHeader();
Status storeSaveHeader();
Status storeAppend(const MeasurementRecord& record);
Status storeReadAt(uint16_t index, MeasurementRecord& recordOut);
Status storeClearLogical();
Status storeClearPhysical();

// ===== 4. CHU TRINH CHINH: setup(), loop(), va cac buoc do =====

// =============================================================================
//  Khoi dong
// =============================================================================

void setup() {
  const uint32_t nowMs = millis();

  settingsBegin();

  Wire.begin(cfg::pin::kI2cSda, cfg::pin::kI2cScl, cfg::i2c::kClockHz);

  ledsBegin();
  buttonBegin(startButton);
  buttonBegin(resetButton);

  devLcd = isOk(displayBegin());
  displayShowBoot();

  heightBegin();
  heightMountHeightCm = settingsMountHeightCm;

  weightBegin();
  weightSetCalibration(settingsWeightOffset, settingsWeightScale);

  devPulseOx = isOk(pulseBegin());

  devEeprom = isOk(eepromBegin());
  storeBegin();

  linkBegin();

  state          = AppState::kSelfTest;
  stateEnteredMs = nowMs;

  emitSelfTest();
}

// =============================================================================
//  Vong lap chinh
// =============================================================================

void loop() {
  const uint32_t nowMs = millis();

  linkUpdate(nowMs);
  ledsUpdate(nowMs);

  // Nut RESET uu tien tuyet doi: bam luc nao cung ve IDLE.
  if (buttonUpdate(resetButton, nowMs)) {
    enterIdle(nowMs);
    return;
  }

  switch (state) {
    case AppState::kBoot:
      break;

    case AppState::kSelfTest:
      // Cho cam bien on dinh sau khi cap nguon roi moi cho phep do.
      if (nowMs - stateEnteredMs >= 1500) {
        // Tare luc khoi dong bu duoc troi cua loadcell. Neu that bai (co nguoi
        // dang dung tren ban can) thi giu nguyen he so da luu trong NVS.
        weightTare();
        emitInfo();
        enterIdle(nowMs);
      }
      break;

    case AppState::kIdle:
      if (buttonUpdate(startButton, nowMs)) enterMeasuring(nowMs);
      break;

    case AppState::kMeasurePpg:
      stepPulseOx(nowMs);
      break;

    case AppState::kMeasureHeight:
      stepHeight(nowMs);
      break;

    case AppState::kMeasureWeight:
      stepWeight(nowMs);
      break;

    case AppState::kDone:
      if (buttonUpdate(startButton, nowMs)) enterMeasuring(nowMs);
      break;

    case AppState::kError:
      if (nowMs - stateEnteredMs >= cfg::flow::kErrorHoldMs) enterIdle(nowMs);
      break;
  }

  serviceStore(nowMs);
  serviceHeartbeat(nowMs);
  emitState(nowMs);
}

// =============================================================================
//  Chuyen trang thai
// =============================================================================

void enterIdle(uint32_t nowMs) {
  state          = AppState::kIdle;
  stateEnteredMs = nowMs;
  lastError      = Status::kOk;
  record.clear();

  linkMeasuring = false;
  ledsSet(LedPattern::kIdle);
  displayShowIdle();
  emitStateNow();
}

void enterMeasuring(uint32_t nowMs) {
  state          = AppState::kMeasurePpg;
  stateEnteredMs = nowMs;
  record.clear();

  linkMeasuring = true;
  ledsSet(LedPattern::kMeasuring);
  displayShowMeasuring("Nhip tim/SpO2", 0);

  pulseStart(nowMs);
  emitStateNow();
}

void enterDone(uint32_t nowMs) {
  state          = AppState::kDone;
  stateEnteredMs = nowMs;

  record.epochSec = clockNowEpoch(nowMs);
  record.set(flag::kTimeValid, clockValid);
  record.computeBmi();

  linkMeasuring = false;
  ledsSet(LedPattern::kDone);
  displayShowResult(record);

  emitStateNow();
  publishRecord(record, RecordSource::kLive);
}

void enterError(Status status, uint32_t nowMs) {
  state          = AppState::kError;
  stateEnteredMs = nowMs;
  lastError      = status;

  linkMeasuring = false;
  ledsSet(LedPattern::kError);
  displayShowError(toString(status));

  emitError(status);
}

// =============================================================================
//  Cac buoc do
// =============================================================================

void stepPulseOx(uint32_t nowMs) {
  const PulsePhase phase = pulseUpdate(nowMs);

  if (phase == PulsePhase::kPaused) {
    displayShowPaused();
  } else if (phase != PulsePhase::kFinished) {
    displayShowMeasuring("Nhip tim/SpO2", pulseProgressPercent(nowMs));
  }

  // Han cho ca buoc: nguoi do bo di giua chung thi khong treo mai.
  if (nowMs - stateEnteredMs > cfg::flow::kStepPulseOxTimeoutMs) {
    enterError(Status::kTimeout, nowMs);
    return;
  }

  if (phase != PulsePhase::kFinished) return;

  uint8_t heartRate = 0;
  uint8_t spo2      = 0;
  if (isOk(pulseResult(heartRate, spo2))) {
    record.heartRate = heartRate;
    record.spo2      = spo2;
    record.set(flag::kHeartValid, true);
    record.set(flag::kSpo2Valid, true);
  }
  // Khong doc duoc nhip tim khong lam hong ca phep do: van do tiep chieu cao va
  // can nang, hai truong do se duoc danh dau khong hop le.

  state          = AppState::kMeasureHeight;
  stateEnteredMs = nowMs;
  displayShowMeasuring("Chieu cao", 70);
}

void stepHeight(uint32_t nowMs) {
  float heightCm = 0.0f;
  const Status status = heightMeasure(heightCm);
  if (isOk(status)) {
    record.heightCm = heightCm;
    record.set(flag::kHeightValid, true);
  }

  state          = AppState::kMeasureWeight;
  stateEnteredMs = millis();
  displayShowMeasuring("Can nang", 88);
  (void)nowMs;
}

void stepWeight(uint32_t nowMs) {
  float weightKg = 0.0f;
  const Status status = weightMeasure(weightKg);
  if (isOk(status)) {
    record.weightKg = weightKg;
    record.set(flag::kWeightValid, true);
  }

  // Yeu cau du an: khong doc duoc gia tri nao thi coi la loi va tu dat lai.
  if (!record.has(flag::kWeightValid) && !record.has(flag::kHeightValid) &&
      !record.has(flag::kHeartValid)) {
    enterError(isOk(status) ? Status::kNoisy : status, millis());
    return;
  }

  enterDone(millis());
  (void)nowMs;
}

// =============================================================================
//  Giao tiep
// =============================================================================

void handleCommand(const Command& command) {
  const uint32_t nowMs = millis();

  switch (command.type) {
    case CommandType::kSync:
      clockSync(command.epoch, nowMs);
      emitSelfTest();   // giao dien vua noi vao, cho biet thiet bi nao thieu
      emitInfo();
      break;

    case CommandType::kMeasure:
      if (state == AppState::kIdle || state == AppState::kDone ||
          state == AppState::kError) {
        enterMeasuring(nowMs);
      }
      break;

    case CommandType::kReset:
      // Reset MEM: dua trang thai ve dau nhung giu nguyen ket noi va du lieu
      // dang cho trong EEPROM. Ban PIC goi reset_cpu() nen mat ca hai.
      enterIdle(nowMs);
      break;

    case CommandType::kFlush:
      if (storeCount > 0) flushing = true;
      break;

    case CommandType::kCalibrate:
      handleCalibration(command);
      break;

    case CommandType::kConfig:
      if (command.ssid[0] != '\0') {
        settingsSetWifiCredentials(command.ssid, command.password);
        wifiBegin();   // ap dung ngay voi thong tin moi
        emitInfo();
      }
      break;

    case CommandType::kEraseStore: {
      const Status status = command.physicalErase ? storeClearPhysical()
                                                  : storeClearLogical();
      if (!isOk(status)) emitError(status);
      emitInfo();
      break;
    }

    default:
      break;
  }
}

void handleCalibration(const Command& command) {
  Status status = Status::kInvalidArg;

  switch (command.target) {
    case CalibrationTarget::kWeightTare:
      status = weightTare();
      if (isOk(status)) {
        settingsSetWeightCalibration(weightOffset, weightScale);
      }
      break;

    case CalibrationTarget::kWeightKnown:
      status = weightCalibrateWith(command.value);
      if (isOk(status)) {
        settingsSetWeightCalibration(weightOffset, weightScale);
      }
      break;

    case CalibrationTarget::kHeightMount:
      if (command.value > 0.0f) {
        settingsSetMountHeightCm(command.value);
        heightMountHeightCm = command.value;
        status = Status::kOk;
      }
      break;

    default:
      break;
  }

  if (isOk(status)) {
    emitInfo();
  } else {
    emitError(status);
  }
}

void emitState(uint32_t nowMs) {
  // Chi ban tin tien trinh moi can phat lien tuc; trang thai tinh da duoc phat
  // mot lan luc chuyen bang emitStateNow().
  const bool measuring = (state == AppState::kMeasurePpg ||
                          state == AppState::kMeasureHeight ||
                          state == AppState::kMeasureWeight);
  if (!measuring) return;
  if (nowMs - lastProgressMs < cfg::flow::kProgressIntervalMs) return;
  lastProgressMs = nowMs;

  const size_t length = encodeState(g_outBuffer, sizeof(g_outBuffer),
                                              stateName(), stepName(),
                                              progressPercent(nowMs), etaSeconds(nowMs));
  if (length > 0) linkSend(g_outBuffer);
}

void emitStateNow() {
  const uint32_t nowMs = millis();
  lastProgressMs = nowMs;
  const size_t length = encodeState(g_outBuffer, sizeof(g_outBuffer),
                                              stateName(), stepName(),
                                              progressPercent(nowMs), etaSeconds(nowMs));
  if (length > 0) linkSend(g_outBuffer);
}

void emitError(Status status) {
  const size_t length = encodeError(g_outBuffer, sizeof(g_outBuffer),
                                              toString(status), "Buoc do that bai");
  if (length > 0) linkSend(g_outBuffer);
}

void emitInfo() {
  const size_t length = encodeInfo(g_outBuffer, sizeof(g_outBuffer),
                                             linkActiveName(),
                                             wifiIp,
                                             storeCount,
                                             heightMountHeightCm,
                                             weightScale);
  if (length > 0) linkSend(g_outBuffer);
}

void emitSelfTest() {
  const size_t length = encodeSelfTest(g_outBuffer, sizeof(g_outBuffer),
                                                 devLcd, devPulseOx, devEeprom);
  if (length == 0) return;

  // Gui thang ra UART khi boot, vi chua nhan ping de chon kenh truyen.
  uartSend(g_outBuffer, strlen(g_outBuffer));
}

void publishRecord(const MeasurementRecord& record, RecordSource source) {
  const size_t length = encodeResult(g_outBuffer, sizeof(g_outBuffer),
                                               record, source);
  if (length == 0) return;

  if (linkSend(g_outBuffer)) return;

  // Khong con kenh nao: cat vao EEPROM de day len khi noi lai duoc.
  const Status status = storeAppend(record);
  if (!isOk(status)) {
    // Khong con cho luu — van tiep tuc chay, chi mat ban ghi nay.
    lastError = status;
  }
}

void serviceStore(uint32_t nowMs) {
  // Vua co ket noi tro lai va con ban ghi ton dong -> tu dong day len.
  if (!flushing) {
    if (linkState != LinkState::kNoLink && storeCount > 0) {
      flushing   = true;
      flushIndex = 0;
    }
    return;
  }

  if (linkState == LinkState::kNoLink) {
    flushing = false;   // mat ket noi giua chung: lan sau day lai tu ban dau tien
    return;
  }

  // Gian nhip de khong lam nghen kenh truyen va khong lam tre chu trinh do.
  if (nowMs - lastFlushMs < kFlushIntervalMs) return;
  lastFlushMs = nowMs;

  // Da day het: yeu cau du an la phai xoa sach EEPROM sau khi truyen xong.
  if (flushIndex >= storeCount) {
    storeClearLogical();
    flushing = false;
    emitInfo();
    return;
  }

  MeasurementRecord stored;
  const Status status = storeReadAt(flushIndex, stored);
  if (status == Status::kCrcError) {
    ++flushIndex;   // bo qua ban ghi hong, van day nhung ban con lai
    return;
  }
  if (!isOk(status)) {
    flushing = false;   // loi bus: thu lai o lan sau
    return;
  }

  const size_t length = encodeResult(g_outBuffer, sizeof(g_outBuffer),
                                               stored, RecordSource::kEeprom);
  if (length == 0) {
    ++flushIndex;
    return;
  }
  if (!linkSend(g_outBuffer)) {
    flushing = false;   // kenh vua rot: giu nguyen du lieu, day lai sau
    return;
  }

  ++flushIndex;
}

void serviceHeartbeat(uint32_t nowMs) {
  if (nowMs - lastPongMs < cfg::link::kHeartbeatMs) return;
  lastPongMs = nowMs;

  const size_t length = encodePong(g_outBuffer, sizeof(g_outBuffer),
                                             linkActiveName(), nowMs / 1000UL,
                                             storeCount);
  if (length > 0) linkSend(g_outBuffer);
}

// =============================================================================
//  Nhan hieu trang thai
// =============================================================================

const char* stateName() {
  switch (state) {
    case AppState::kBoot:          return "boot";
    case AppState::kSelfTest:      return "selftest";
    case AppState::kIdle:          return "idle";
    case AppState::kMeasurePpg:
    case AppState::kMeasureHeight:
    case AppState::kMeasureWeight: return "measuring";
    case AppState::kDone:          return "done";
    case AppState::kError:         return "error";
  }
  return "idle";
}

const char* stepName() {
  switch (state) {
    case AppState::kMeasurePpg:    return pulsePhase == PulsePhase::kPaused ? "paused" : "ppg";
    case AppState::kMeasureHeight: return "height";
    case AppState::kMeasureWeight: return "weight";
    default:                       return "none";
  }
}

uint8_t progressPercent(uint32_t nowMs) {
  switch (state) {
    // PPG chiem 0-70% tong chu trinh, chieu cao 70-88%, can nang 88-100%.
    case AppState::kMeasurePpg:
      return static_cast<uint8_t>(pulseProgressPercent(nowMs) * 70UL / 100UL);
    case AppState::kMeasureHeight: return 78;
    case AppState::kMeasureWeight: return 94;
    case AppState::kDone:          return 100;
    default:                       return 0;
  }
}

uint16_t etaSeconds(uint32_t nowMs) {
  if (state != AppState::kMeasurePpg) {
    return (state == AppState::kMeasureHeight || state == AppState::kMeasureWeight) ? 1 : 0;
  }
  const uint8_t done = pulseProgressPercent(nowMs);
  const uint32_t remainMs = (cfg::pulseox::kMeasureMs * (100UL - done)) / 100UL;
  return static_cast<uint16_t>((remainMs + 999UL) / 1000UL) + 2;   // cong 2 s cho hai buoc sau
}

// ===== 5. NUT NHAN: chong doi bang millis() =====

void buttonBegin(Button& button) {
  pinMode(button.pin, INPUT_PULLUP);
  button.stable  = false;
  button.lastRaw = false;
}

bool buttonUpdate(Button& button, uint32_t nowMs) {
  // Nut noi xuong GND: doc LOW nghia la dang bam.
  const bool raw = (digitalRead(button.pin) == LOW);

  if (raw != button.lastRaw) {
    button.lastRaw      = raw;
    button.lastChangeMs = nowMs;
    return false;
  }

  if (nowMs - button.lastChangeMs < cfg::button::kDebounceMs) return false;
  if (raw == button.stable) return false;

  button.stable = raw;
  if (!button.stable) return false;  // chi tinh suon xuong (luc bat dau bam)

  if (nowMs - button.lastAcceptMs < cfg::button::kRepeatLockMs) return false;
  button.lastAcceptMs = nowMs;
  return true;
}

// ===== 6. DEN BAO TRANG THAI =====

void ledsBegin() {
  pinMode(cfg::pin::kLedRed, OUTPUT);
  pinMode(cfg::pin::kLedGreen, OUTPUT);
  ledsSet(LedPattern::kIdle);
}

void ledsSet(LedPattern pattern) {
  if (ledPattern == pattern) return;
  ledPattern    = pattern;
  ledBlinkRefMs = millis();

  switch (ledPattern) {
    case LedPattern::kIdle:      ledsWrite(false, false); break;
    case LedPattern::kMeasuring: ledsWrite(true,  false); break;
    case LedPattern::kDone:      ledsWrite(false, true);  break;
    case LedPattern::kError:     ledsWrite(true,  false); break;
  }
}

void ledsUpdate(uint32_t nowMs) {
  if (ledPattern != LedPattern::kMeasuring) return;

  const uint32_t phase = (nowMs - ledBlinkRefMs) % cfg::led::kBlinkPeriodMs;
  ledsWrite(phase < cfg::led::kBlinkPeriodMs / 2, false);
}

void ledsWrite(bool red, bool green) {
  digitalWrite(cfg::pin::kLedRed, red ? HIGH : LOW);
  digitalWrite(cfg::pin::kLedGreen, green ? HIGH : LOW);
}

// ===== 7. MAN HINH LCD 16x2 =====

Status displayBegin() {
  displayPresent = (g_lcd.begin(cfg::lcd::kCols, cfg::lcd::kRows) == 0);
  if (!displayPresent) return Status::kNotPresent;

  g_lcd.backlight();
  g_lcd.clear();
  memset(displayShadow, 0, sizeof(displayShadow));
  return Status::kOk;
}

void displayWriteLine(uint8_t row, const char* text) {
  if (!displayPresent || row >= cfg::lcd::kRows) return;

  char padded[cfg::lcd::kCols + 1];
  snprintf(padded, sizeof(padded), "%-*s", cfg::lcd::kCols, text);

  if (strcmp(padded, displayShadow[row]) == 0) return;   // khong doi -> khong ve lai

  strcpy(displayShadow[row], padded);
  g_lcd.setCursor(0, row);
  g_lcd.print(padded);
}

void displayShowBoot() {
  displayWriteLine(0, "SATx  Khoi dong");
  displayWriteLine(1, "Vui long cho...");
}

void displayShowIdle() {
  displayWriteLine(0, "San sang");
  displayWriteLine(1, "Bam START de do");
}

void displayShowMeasuring(const char* stepLabel, uint8_t percent) {
  char line[cfg::lcd::kCols + 1];
  snprintf(line, sizeof(line), "Dang do %3u%%", static_cast<unsigned>(percent));
  displayWriteLine(0, line);
  displayWriteLine(1, stepLabel);
}

void displayShowPaused() {
  displayWriteLine(0, "Tam dung");
  displayWriteLine(1, "Dat lai ngon tay");
}

void displayShowResult(const MeasurementRecord& r) {
  char line[cfg::lcd::kCols + 1];

  // Dong 1: can nang + chieu cao. Dong 2: nhip tim + SpO2.
  // Truong khong hop le hien "--" thay vi so 0 gia.
  if (r.has(flag::kWeightValid) && r.has(flag::kHeightValid)) {
    snprintf(line, sizeof(line), "W%5.1f H%5.1f", r.weightKg, r.heightCm);
  } else if (r.has(flag::kWeightValid)) {
    snprintf(line, sizeof(line), "W%5.1f H  --", r.weightKg);
  } else if (r.has(flag::kHeightValid)) {
    snprintf(line, sizeof(line), "W  --  H%5.1f", r.heightCm);
  } else {
    snprintf(line, sizeof(line), "W  --  H  --");
  }
  displayWriteLine(0, line);

  char hr[4] = "--";
  char sp[4] = "--";
  if (r.has(flag::kHeartValid)) snprintf(hr, sizeof(hr), "%3u", static_cast<unsigned>(r.heartRate));
  if (r.has(flag::kSpo2Valid))  snprintf(sp, sizeof(sp), "%3u", static_cast<unsigned>(r.spo2));
  snprintf(line, sizeof(line), "B:%s S:%s%%", hr, sp);
  displayWriteLine(1, line);
}

void displayShowError(const char* code) {
  char line[cfg::lcd::kCols + 1];
  snprintf(line, sizeof(line), "%s", code);
  displayWriteLine(0, "LOI DO");
  displayWriteLine(1, line);
}

// ===== 8. CHIEU CAO: HC-SR04, loc trung vi =====

// Sap xep chen — n <= 5 nen don gian la du va nhanh nhat.
void sortAscending(float* values, uint8_t count) {
  for (uint8_t i = 1; i < count; ++i) {
    const float key = values[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0 && values[j] > key) {
      values[j + 1] = values[j];
      --j;
    }
    values[j + 1] = key;
  }
}

void heightBegin() {
  pinMode(cfg::pin::kUltrasonicTrig, OUTPUT);
  pinMode(cfg::pin::kUltrasonicEcho, INPUT);
  digitalWrite(cfg::pin::kUltrasonicTrig, LOW);
  heightMountHeightCm = cfg::height::kDefaultMountHeightCm;
}

Status heightReadDistanceOnce(float& distanceCmOut) {
  digitalWrite(cfg::pin::kUltrasonicTrig, LOW);
  delayMicroseconds(4);
  digitalWrite(cfg::pin::kUltrasonicTrig, HIGH);
  delayMicroseconds(cfg::height::kTrigPulseUs);
  digitalWrite(cfg::pin::kUltrasonicTrig, LOW);

  // pulseIn tu cho suon len roi do den suon xuong (hoac nguoc lai neu opto dao).
  const uint32_t echoUs = pulseIn(cfg::pin::kUltrasonicEcho,
                                  cfg::height::kEchoActiveLevel ? HIGH : LOW,
                                  cfg::height::kEchoTimeoutUs);
  if (echoUs == 0) return Status::kTimeout;

  const float cm = static_cast<float>(echoUs) / cfg::height::kUsPerCm;
  if (cm < cfg::height::kMinDistanceCm || cm > cfg::height::kMaxDistanceCm) return Status::kOutOfRange;

  distanceCmOut = cm;
  return Status::kOk;
}

Status heightMeasure(float& heightCmOut) {
#if SATX_SIMULATE_SENSORS
  heightLastDistanceCm = heightMountHeightCm - (158.0f + static_cast<float>(millis() % 300) / 10.0f);
  heightCmOut     = heightMountHeightCm - heightLastDistanceCm;
  delay(200);   // giu dung nhip thoi gian nhu khi do that
  return Status::kOk;
#else
  float samples[cfg::height::kSampleCount];
  uint8_t valid = 0;

  for (uint8_t i = 0; i < cfg::height::kSampleCount; ++i) {
    float cm = 0.0f;
    if (isOk(heightReadDistanceOnce(cm))) samples[valid++] = cm;
    delay(cfg::height::kSampleGapMs);
  }

  // Can qua nua so mau hop le thi ket qua moi dang tin.
  if (valid < (cfg::height::kSampleCount / 2 + 1)) return Status::kNoisy;

  // Trung vi khu duoc mau ngoai lai do nguoi dich chuyen, tot hon trung binh.
  sortAscending(samples, valid);
  heightLastDistanceCm = samples[valid / 2];

  const float height = heightMountHeightCm - heightLastDistanceCm;
  if (height < cfg::height::kMinHeightCm || height > cfg::height::kMaxHeightCm) return Status::kOutOfRange;

  heightCmOut = height;
  return Status::kOk;
#endif
}

// ===== 9. CAN NANG: HX711, cat mau ngoai lai va hieu chuan =====

void weightBegin() {
  g_hx711.begin(cfg::pin::kLoadCellData, cfg::pin::kLoadCellClock);
  weightOffset = 0.0f;
  weightScale  = cfg::weight::kDefaultScale;
}

void weightSetCalibration(float offset, float scale) {
  weightOffset = offset;
  if (scale != 0.0f) weightScale = scale;
}

Status weightReadFilteredRaw(float& rawOut) {
#if SATX_SIMULATE_SENSORS
  delay(150);
  rawOut = weightOffset + weightScale * (52.0f + static_cast<float>(millis() % 250) / 10.0f);
  return Status::kOk;
#else
  float samples[cfg::weight::kSampleCount];
  uint8_t valid = 0;

  const uint32_t deadline = millis() + cfg::weight::kReadTimeoutMs;
  while (valid < cfg::weight::kSampleCount) {
    if (static_cast<int32_t>(millis() - deadline) >= 0) break;
    if (!g_hx711.is_ready()) {
      delay(1);
      continue;
    }
    samples[valid++] = static_cast<float>(g_hx711.read());
  }

  // Can du mau de con lai it nhat mot mau sau khi cat hai dau.
  if (valid < (2 * cfg::weight::kTrimCount + 1)) return Status::kTimeout;

  sortAscending(samples, valid);

  float sum = 0.0f;
  const uint8_t first = cfg::weight::kTrimCount;
  const uint8_t last  = static_cast<uint8_t>(valid - cfg::weight::kTrimCount);
  for (uint8_t i = first; i < last; ++i) sum += samples[i];

  rawOut = sum / static_cast<float>(last - first);
  return Status::kOk;
#endif
}

Status weightMeasure(float& weightKgOut) {
  if (weightScale == 0.0f) return Status::kNotCalibrated;

  float raw = 0.0f;
  const Status status = weightReadFilteredRaw(raw);
  if (!isOk(status)) return status;

  const float kg = (raw - weightOffset) / weightScale;
  if (kg < cfg::weight::kMinWeightKg || kg > cfg::weight::kMaxWeightKg) return Status::kOutOfRange;

  weightKgOut = kg;
  return Status::kOk;
}

Status weightTare() {
  float raw = 0.0f;
  const Status status = weightReadFilteredRaw(raw);
  if (!isOk(status)) return status;
  weightOffset = raw;
  return Status::kOk;
}

Status weightCalibrateWith(float knownKg) {
  if (knownKg <= 0.0f) return Status::kInvalidArg;

  float raw = 0.0f;
  const Status status = weightReadFilteredRaw(raw);
  if (!isOk(status)) return status;

  const float delta = raw - weightOffset;
  if (fabsf(delta) < 1.0f) return Status::kNoisy;   // qua can chua len ban can?

  weightScale = delta / knownKg;
  return Status::kOk;
}

// ===== 10. NHIP TIM / SpO2: MAX30102, tam dung khi roi ngon tay =====

uint8_t medianOf(const uint8_t* values, uint8_t count) {
  uint8_t sorted[16];
  memcpy(sorted, values, count);
  for (uint8_t i = 1; i < count; ++i) {
    const uint8_t key = sorted[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = key;
  }
  return sorted[count / 2];
}

Status pulseBegin() {
#if SATX_SIMULATE_SENSORS
  pulsePresent = true;
  return Status::kOk;
#else
  // MAX30105::begin() tu goi Wire.setClock(): phai giu toc do chung cho LCD.
  pulsePresent = g_sensor.begin(Wire, cfg::i2c::kClockHz, cfg::i2c::kAddrPulseOx);
  if (!pulsePresent) return Status::kNotPresent;

  g_sensor.setup(cfg::pulseox::kLedBrightness, cfg::pulseox::kSampleAverage, cfg::pulseox::kLedMode,
                 cfg::pulseox::kSampleRateHz, cfg::pulseox::kPulseWidthUs, cfg::pulseox::kAdcRange);
  return Status::kOk;
#endif
}

void pulseStart(uint32_t nowMs) {
  pulseFilled        = 0;
  pulseEstimateCount = 0;
  pulseSampledMs     = 0;
  pulseFingerPresent = false;
  pulseFingerChangeMs = 0;
  pulsePhase         = PulsePhase::kWarmup;
  pulsePhaseStartMs  = nowMs;
}

void pulseResetWindow(uint32_t nowMs) {
  // Ngon tay vua dat lai: bo loc DC phai on dinh lai tu dau, giong ban PIC.
  pulseFilled       = 0;
  pulsePhase        = PulsePhase::kWarmup;
  pulsePhaseStartMs = nowMs;
}

void pulseDetectFinger(uint32_t irValue, uint32_t nowMs) {
  const bool wantPresent = pulseFingerPresent ? (irValue >= cfg::pulseox::kFingerOffThreshold)
                                          : (irValue > cfg::pulseox::kFingerOnThreshold);
  if (wantPresent == pulseFingerPresent) {
    pulseFingerChangeMs = 0;
    return;
  }
  if (pulseFingerChangeMs == 0) {
    pulseFingerChangeMs = nowMs;
    return;
  }
  if (nowMs - pulseFingerChangeMs < cfg::pulseox::kFingerDebounceMs) return;

  pulseFingerPresent  = wantPresent;
  pulseFingerChangeMs = 0;
}

void pulsePullFifoSamples() {
#if !SATX_SIMULATE_SENSORS
  g_sensor.check();
  while (g_sensor.available()) {
    if (pulseFilled < kBufferLen) {
      pulseRedBuffer[pulseFilled] = g_sensor.getFIFORed();
      pulseIrBuffer[pulseFilled]  = g_sensor.getFIFOIR();
      ++pulseFilled;
    } else {
      // Bo dem day: truot cua so di kShiftLen mau roi nap tiep.
      memmove(pulseRedBuffer, pulseRedBuffer + kShiftLen, (kBufferLen - kShiftLen) * sizeof(uint32_t));
      memmove(pulseIrBuffer,  pulseIrBuffer  + kShiftLen, (kBufferLen - kShiftLen) * sizeof(uint32_t));
      pulseFilled = kBufferLen - kShiftLen;
    }
    g_sensor.nextSample();
  }
#endif
}

void pulseRunAlgorithmIfReady() {
#if !SATX_SIMULATE_SENSORS
  if (pulseFilled < kBufferLen) return;
  if (pulseEstimateCount >= kMaxEstimates) return;

  int32_t spo2 = 0, heartRate = 0;
  int8_t  spo2Valid = 0, heartRateValid = 0;

  maxim_heart_rate_and_oxygen_saturation(pulseIrBuffer, kBufferLen, pulseRedBuffer,
                                         &spo2, &spo2Valid,
                                         &heartRate, &heartRateValid);

  const bool hrOk = heartRateValid && heartRate >= cfg::pulseox::kMinHeartRate &&
                    heartRate <= cfg::pulseox::kMaxHeartRate;
  const bool spOk = spo2Valid && spo2 >= cfg::pulseox::kMinSpo2 && spo2 <= cfg::pulseox::kMaxSpo2;
  if (!hrOk || !spOk) return;

  pulseHrEstimates[pulseEstimateCount]   = static_cast<uint8_t>(heartRate);
  pulseSpo2Estimates[pulseEstimateCount] = static_cast<uint8_t>(spo2);
  ++pulseEstimateCount;
#endif
}

PulsePhase pulseUpdate(uint32_t nowMs) {
  if (pulsePhase == PulsePhase::kIdle || pulsePhase == PulsePhase::kFinished) return pulsePhase;

#if SATX_SIMULATE_SENSORS
  pulseFingerPresent = true;
  const uint32_t elapsed = nowMs - pulsePhaseStartMs;
  if (pulsePhase == PulsePhase::kWarmup) {
    if (elapsed < cfg::pulseox::kWarmupMs) return pulsePhase;
    pulsePhase        = PulsePhase::kSampling;
    pulsePhaseStartMs = nowMs;
    return pulsePhase;
  }
  pulseSampledMs = nowMs - pulsePhaseStartMs;
  if (pulseSampledMs >= cfg::pulseox::kMeasureMs) {
    pulseHrEstimates[0]   = static_cast<uint8_t>(62 + (nowMs % 29));
    pulseSpo2Estimates[0] = static_cast<uint8_t>(95 + (nowMs % 4));
    pulseEstimateCount    = 1;
    pulsePhase            = PulsePhase::kFinished;
  }
  return pulsePhase;
#else
  pulsePullFifoSamples();

  const uint32_t latestIr = (pulseFilled > 0) ? pulseIrBuffer[pulseFilled - 1] : 0;
  pulseDetectFinger(latestIr, nowMs);

  if (!pulseFingerPresent) {
    if (pulsePhase != PulsePhase::kPaused) {
      pulseSampledMs += (pulsePhase == PulsePhase::kSampling) ? (nowMs - pulsePhaseStartMs) : 0;
      pulsePhase = PulsePhase::kPaused;
    }
    return pulsePhase;
  }

  if (pulsePhase == PulsePhase::kPaused) {
    pulseResetWindow(nowMs);
    return pulsePhase;
  }

  if (pulsePhase == PulsePhase::kWarmup) {
    if (nowMs - pulsePhaseStartMs < cfg::pulseox::kWarmupMs) return pulsePhase;
    pulsePhase        = PulsePhase::kSampling;
    pulsePhaseStartMs = nowMs;
    return pulsePhase;
  }

  pulseRunAlgorithmIfReady();

  if (pulseSampledMs + (nowMs - pulsePhaseStartMs) >= cfg::pulseox::kMeasureMs) {
    pulseSampledMs += (nowMs - pulsePhaseStartMs);
    pulsePhase = PulsePhase::kFinished;
  }
  return pulsePhase;
#endif
}

uint8_t pulseProgressPercent(uint32_t nowMs) {
  uint32_t elapsed = pulseSampledMs;
  if (pulsePhase == PulsePhase::kSampling) elapsed += (nowMs - pulsePhaseStartMs);
  if (elapsed >= cfg::pulseox::kMeasureMs) return 100;
  return static_cast<uint8_t>(elapsed * 100UL / cfg::pulseox::kMeasureMs);
}

Status pulseResult(uint8_t& heartRateOut, uint8_t& spo2Out) {
  if (pulsePhase != PulsePhase::kFinished) return Status::kNotReady;
  if (pulseEstimateCount == 0) return Status::kNoisy;

  // Trung vi cua cac uoc luong hop le — chong duoc mot vai lan tinh bi lech.
  heartRateOut = medianOf(pulseHrEstimates, pulseEstimateCount);
  spo2Out      = medianOf(pulseSpo2Estimates, pulseEstimateCount);
  return Status::kOk;
}

// ===== 11. KET NOI: UART uu tien, WiFi du phong =====

// Dong qua dai bi bo tron ven; dong tiep theo van duoc nhan binh thuong.
bool lineFeed(LineBuffer& line, char c) {
  if (c == '\n' || c == '\r') {
    if (line.length == 0) return false;
    line.buffer[line.length] = '\0';
    const bool ready = !line.overflow;
    line.length = 0;
    line.overflow = false;
    return ready;
  }
  if (line.length + 1 >= cfg::link::kMaxLineLen) {
    line.overflow = true;
    return false;
  }
  line.buffer[line.length++] = c;
  return false;
}

void lineReset(LineBuffer& line) {
  line.length = 0;
  line.overflow = false;
}

void uartBegin() {
#if SATX_LINK_UART_NUM == 2
  linkPort.begin(cfg::link::kUartBaud, SERIAL_8N1, cfg::pin::kUart2Rx, cfg::pin::kUart2Tx);
#else
  linkPort.begin(cfg::link::kUartBaud);
#endif
  lineReset(uartBuffer);
}

bool uartSend(const char* line, size_t length) {
  return linkPort.write(reinterpret_cast<const uint8_t*>(line), length) == length;
}

bool uartIsPeerAlive(uint32_t nowMs) {
  return uartSeenPeer && (nowMs - uartLastPeerMs) < cfg::link::kUartTimeoutMs;
}

void wifiBegin() {
  if (settingsSsid[0] == '\0') {
    wifiStarted = false;
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settingsSsid, settingsPassword);
  // Giu ket noi san voi router; chuyen kenh chi can danh thuc modem.
  WiFi.setSleep(true);
  wifiActive = false;
  g_server.begin();
  g_server.onEvent(onWebSocketEvent);
  wifiStarted = true;
  lineReset(wifiBuffer);
}

void wifiSetActive(bool active) {
  if (!wifiStarted || wifiActive == active) return;
  wifiActive = active;
  WiFi.setSleep(!active);
}

void wifiUpdate() {
  if (!wifiStarted) return;
  g_server.loop();
  if (WiFi.status() == WL_CONNECTED) {
    const String current = WiFi.localIP().toString();
    if (current != wifiIp) {
      strlcpy(wifiIp, current.c_str(), sizeof(wifiIp));
      MDNS.end();
      if (MDNS.begin(cfg::link::kMdnsHostname)) {
        MDNS.addService("ws", "tcp", cfg::link::kWebSocketPort);
      }
    }
  }
}

bool wifiSend(const char* line, size_t length) {
  if (!wifiStarted || wifiClientCount == 0) return false;
  return g_server.broadcastTXT(reinterpret_cast<const uint8_t*>(line), length);
}

bool wifiIsPeerAlive(uint32_t nowMs) {
  return wifiStarted && wifiClientCount > 0 && wifiSeenPeer &&
         (nowMs - wifiLastPeerMs) < cfg::link::kWifiTimeoutMs;
}

void handleLine(bool fromUart, const char* line, uint32_t nowMs) {
  Command command;
  if (!parseCommand(line, command)) return;
  // Moi lenh hop le deu la dau hieu giao dien con song, ke ca ping.
  if (fromUart) {
    uartLastPeerMs = nowMs;
    uartSeenPeer = true;
  } else {
    wifiLastPeerMs = nowMs;
    wifiSeenPeer = true;
  }
  if (command.type == CommandType::kPing) return;

  // Chi giao dien tren kenh dang chon duoc dieu khien may.
  const bool fromActive = (linkState == LinkState::kUartActive && fromUart) ||
                          (linkState == LinkState::kWifiActive && !fromUart) ||
                          (linkState == LinkState::kNoLink);
  if (fromActive) handleCommand(command);
}

void onWebSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length) {
  (void)clientNum;
  switch (type) {
    case WStype_CONNECTED:
      if (wifiClientCount < 255) ++wifiClientCount;
      break;
    case WStype_DISCONNECTED:
      if (wifiClientCount > 0) --wifiClientCount;
      if (wifiClientCount == 0) {
        wifiSeenPeer = false;
        lineReset(wifiBuffer);
      }
      break;
    case WStype_TEXT:
      // Mot khung co the chua nhieu dong, hoac khong co ky tu xuong dong.
      for (size_t i = 0; i < length; ++i) {
        if (lineFeed(wifiBuffer, static_cast<char>(payload[i]))) {
          handleLine(false, wifiBuffer.buffer, linkNowMs);
        }
      }
      if (length > 0 && payload[length - 1] != '\n' && payload[length - 1] != '\r') {
        if (lineFeed(wifiBuffer, '\n')) handleLine(false, wifiBuffer.buffer, linkNowMs);
      }
      break;
    default:
      break;
  }
}

void linkBegin() {
  uartBegin();
  wifiBegin();
  linkState = LinkState::kNoLink;
}

void linkSwitchTo(LinkState next) {
  if (linkState == next) return;
  linkState = next;
  wifiSetActive(next == LinkState::kWifiActive || next == LinkState::kNoLink);
}

void linkUpdate(uint32_t nowMs) {
  linkNowMs = nowMs;
  while (linkPort.available() > 0) {
    if (lineFeed(uartBuffer, static_cast<char>(linkPort.read()))) {
      handleLine(true, uartBuffer.buffer, nowMs);
    }
  }
  wifiUpdate();
  const bool uartAlive = uartIsPeerAlive(nowMs);
  const bool wifiAlive = wifiIsPeerAlive(nowMs);
  switch (linkState) {
    case LinkState::kUartActive:
      if (!uartAlive) linkSwitchTo(wifiAlive ? LinkState::kWifiActive : LinkState::kNoLink);
      break;
    case LinkState::kWifiActive:
      if (uartAlive) {
        // UART tro lai khi dang do: doi do xong moi chuyen kenh.
        if (!linkMeasuring) linkSwitchTo(LinkState::kUartActive);
      } else if (!wifiAlive) {
        linkSwitchTo(LinkState::kNoLink);
      }
      break;
    case LinkState::kNoLink:
      if (uartAlive) linkSwitchTo(LinkState::kUartActive);
      else if (wifiAlive) linkSwitchTo(LinkState::kWifiActive);
      break;
  }
}

bool linkSend(const char* line) {
  const size_t length = strlen(line);
  switch (linkState) {
    case LinkState::kUartActive: return uartSend(line, length);
    case LinkState::kWifiActive: return wifiSend(line, length);
    case LinkState::kNoLink: return false;
  }
  return false;
}

const char* linkActiveName() {
  switch (linkState) {
    case LinkState::kUartActive: return "uart";
    case LinkState::kWifiActive: return "wifi";
    case LinkState::kNoLink: return "none";
  }
  return "none";
}

// ===== 12. GIAO THUC JSON-LINES: giu tuong thich Web UI =====

CommandType typeFromString(const char* t) {
  if (t == nullptr) return CommandType::kUnknown;
  if (strcmp(t, "ping")    == 0) return CommandType::kPing;
  if (strcmp(t, "sync")    == 0) return CommandType::kSync;
  if (strcmp(t, "measure") == 0) return CommandType::kMeasure;
  if (strcmp(t, "reset")   == 0) return CommandType::kReset;
  if (strcmp(t, "flush")   == 0) return CommandType::kFlush;
  if (strcmp(t, "cal")     == 0) return CommandType::kCalibrate;
  if (strcmp(t, "cfg")     == 0) return CommandType::kConfig;
  if (strcmp(t, "erase")   == 0) return CommandType::kEraseStore;
  return CommandType::kUnknown;
}

bool parseCommand(const char* line, Command& commandOut) {
  if (line == nullptr || line[0] != '{') return false;

  // ArduinoJson 7: JsonDocument tu co gian, gioi han thuc te la do dai dong
  // (cfg::link::kMaxLineLen) da chan o LineAssembler.
  JsonDocument doc;
  if (deserializeJson(doc, line) != DeserializationError::Ok) return false;

  commandOut = Command{};
  commandOut.type = typeFromString(doc["t"] | static_cast<const char*>(nullptr));
  if (commandOut.type == CommandType::kUnknown) return false;

  switch (commandOut.type) {
    case CommandType::kSync:
      commandOut.epoch = doc["epoch"] | 0UL;
      break;

    case CommandType::kCalibrate: {
      const char* target = doc["target"] | "";
      if (strcmp(target, "weight") == 0) {
        // Co known_kg -> tinh he so; khong co -> chi tare.
        if (doc["known_kg"].is<float>()) {
          commandOut.target = CalibrationTarget::kWeightKnown;
          commandOut.value  = doc["known_kg"] | 0.0f;
        } else {
          commandOut.target = CalibrationTarget::kWeightTare;
        }
      } else if (strcmp(target, "height") == 0) {
        commandOut.target = CalibrationTarget::kHeightMount;
        commandOut.value  = doc["mount_cm"] | 0.0f;
      }
      break;
    }

    case CommandType::kConfig: {
      JsonVariantConst wifi = doc["wifi"];
      if (!wifi.isNull()) {
        strlcpy(commandOut.ssid, wifi["ssid"] | "", sizeof(commandOut.ssid));
        strlcpy(commandOut.password, wifi["pass"] | "", sizeof(commandOut.password));
      }
      break;
    }

    case CommandType::kEraseStore:
      commandOut.physicalErase = doc["physical"] | false;
      break;

    default:
      break;
  }
  return true;
}

size_t encodePong(char* out, size_t outSize, const char* linkName,
                  uint32_t uptimeSec, uint16_t pendingRecords) {
  const int n = snprintf(out, outSize,
                         "{\"t\":\"pong\",\"link\":\"%s\",\"up\":%lu,\"pending\":%u}\n",
                         linkName, static_cast<unsigned long>(uptimeSec),
                         static_cast<unsigned>(pendingRecords));
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

size_t encodeState(char* out, size_t outSize, const char* stateName,
                   const char* stepName, uint8_t percent, uint16_t etaSec) {
  const int n = snprintf(out, outSize,
                         "{\"t\":\"state\",\"s\":\"%s\",\"step\":\"%s\","
                         "\"pct\":%u,\"eta\":%u}\n",
                         stateName, stepName, static_cast<unsigned>(percent),
                         static_cast<unsigned>(etaSec));
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

size_t encodeResult(char* out, size_t outSize, const MeasurementRecord& r,
                    RecordSource source) {
  // Truong khong hop le duoc gui la null de giao dien hien "--" thay vi so 0 gia.
  char weight[12] = "null";
  char height[12] = "null";
  char heart[8]   = "null";
  char spo2[8]    = "null";
  char bmi[12]    = "null";

  if (r.has(flag::kWeightValid)) snprintf(weight, sizeof(weight), "%.2f", r.weightKg);
  if (r.has(flag::kHeightValid)) snprintf(height, sizeof(height), "%.1f", r.heightCm);
  if (r.has(flag::kHeartValid))  snprintf(heart, sizeof(heart), "%u", static_cast<unsigned>(r.heartRate));
  if (r.has(flag::kSpo2Valid))   snprintf(spo2, sizeof(spo2), "%u", static_cast<unsigned>(r.spo2));
  if (r.bmi > 0.0f)              snprintf(bmi, sizeof(bmi), "%.1f", r.bmi);

  const int n = snprintf(out, outSize,
                         "{\"t\":\"result\",\"ts\":%lu,\"tsValid\":%s,"
                         "\"w\":%s,\"h\":%s,\"hr\":%s,\"spo2\":%s,\"bmi\":%s,"
                         "\"ok\":%s,\"src\":\"%s\"}\n",
                         static_cast<unsigned long>(r.epochSec),
                         r.has(flag::kTimeValid) ? "true" : "false",
                         weight, height, heart, spo2, bmi,
                         r.isComplete() ? "true" : "false",
                         source == RecordSource::kLive ? "live" : "eeprom");
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

size_t encodeError(char* out, size_t outSize, const char* code, const char* message) {
  const int n = snprintf(out, outSize,
                         "{\"t\":\"error\",\"code\":\"%s\",\"msg\":\"%s\"}\n",
                         code, message);
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

size_t encodeLog(char* out, size_t outSize, const char* level, const char* message) {
  const int n = snprintf(out, outSize,
                         "{\"t\":\"log\",\"lvl\":\"%s\",\"msg\":\"%s\"}\n",
                         level, message);
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

size_t encodeInfo(char* out, size_t outSize, const char* linkName, const char* ip,
                  uint16_t pendingRecords, float mountHeightCm, float weightScale) {
  const int n = snprintf(out, outSize,
                         "{\"t\":\"info\",\"link\":\"%s\",\"ip\":\"%s\",\"pending\":%u,"
                         "\"mount_cm\":%.1f,\"w_scale\":%.2f,\"fw\":\"%s\"}\n",
                         linkName, ip, static_cast<unsigned>(pendingRecords),
                         mountHeightCm, weightScale, "SATx-2.0");
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

size_t encodeSelfTest(char* out, size_t outSize, bool lcd, bool pulseOx, bool eeprom) {
  const int n = snprintf(out, outSize,
                         "{\"t\":\"selftest\",\"lcd\":%s,\"ppg\":%s,\"eeprom\":%s}\n",
                         lcd ? "true" : "false",
                         pulseOx ? "true" : "false",
                         eeprom ? "true" : "false");
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

// ===== 13. DONG HO: dong bo epoch tu may tinh =====

void clockSync(uint32_t epochSec, uint32_t nowMs) {
  epochAtSync = epochSec;
  millisAtSync = nowMs;
  clockValid = (epochSec > 0);
}

uint32_t clockNowEpoch(uint32_t nowMs) {
  if (!clockValid) return 0;
  return epochAtSync + (nowMs - millisAtSync) / 1000UL;
}

// ===== 14. CAU HINH LUU TRONG FLASH NVS =====

constexpr char kNamespace[] = "satx";
constexpr char kKeyOffset[] = "w_off";
constexpr char kKeyScale[]  = "w_scale";
constexpr char kKeyMount[]  = "h_mount";
constexpr char kKeySsid[]   = "wifi_ssid";
constexpr char kKeyPass[]   = "wifi_pass";

void settingsBegin() {
  g_prefs.begin(kNamespace, /*readOnly=*/false);

  settingsWeightOffset  = g_prefs.getFloat(kKeyOffset, 0.0f);
  settingsWeightScale   = g_prefs.getFloat(kKeyScale, cfg::weight::kDefaultScale);
  settingsMountHeightCm = g_prefs.getFloat(kKeyMount, cfg::height::kDefaultMountHeightCm);

  g_prefs.getString(kKeySsid, settingsSsid, sizeof(settingsSsid));
  g_prefs.getString(kKeyPass, settingsPassword, sizeof(settingsPassword));
}

void settingsSetWeightCalibration(float offset, float scale) {
  settingsWeightOffset = offset;
  settingsWeightScale  = scale;
  g_prefs.putFloat(kKeyOffset, offset);
  g_prefs.putFloat(kKeyScale, scale);
}

void settingsSetMountHeightCm(float cm) {
  settingsMountHeightCm = cm;
  g_prefs.putFloat(kKeyMount, cm);
}

void settingsSetWifiCredentials(const char* ssid, const char* password) {
  strlcpy(settingsSsid, ssid ? ssid : "", sizeof(settingsSsid));
  strlcpy(settingsPassword, password ? password : "", sizeof(settingsPassword));
  g_prefs.putString(kKeySsid, settingsSsid);
  g_prefs.putString(kKeyPass, settingsPassword);
}

// ===== 15. EEPROM: doc/ghi I2C, chia trang va cho ACK =====

Status eepromBegin() {
  Wire.beginTransmission(cfg::i2c::kAddrEeprom);
  eepromPresent = (Wire.endTransmission() == 0);
  return eepromPresent ? Status::kOk : Status::kNotPresent;
}

Status eepromWaitWriteComplete() {
  // Ack-polling: chip khong tra ACK trong luc dang ghi noi bo.
  const uint32_t deadline = millis() + 4 * cfg::eeprom::kWriteCycleMs;
  while (static_cast<int32_t>(millis() - deadline) < 0) {
    Wire.beginTransmission(cfg::i2c::kAddrEeprom);
    if (Wire.endTransmission() == 0) return Status::kOk;
    delay(1);
  }
  return Status::kTimeout;
}

Status eepromRead(uint16_t address, uint8_t* buffer, uint16_t length) {
  if (!eepromPresent) return Status::kNotPresent;
  if (buffer == nullptr || length == 0) return Status::kInvalidArg;
  if (static_cast<uint32_t>(address) + length > cfg::eeprom::kCapacityBytes) return Status::kInvalidArg;

  uint16_t done = 0;
  while (done < length) {
    const uint16_t chunk = min<uint16_t>(kChunkBytes, length - done);
    const uint16_t addr  = address + done;

    Wire.beginTransmission(cfg::i2c::kAddrEeprom);
    Wire.write(static_cast<uint8_t>(addr >> 8));
    Wire.write(static_cast<uint8_t>(addr & 0xFF));
    if (Wire.endTransmission(false) != 0) return Status::kIoError;

    const uint8_t got = Wire.requestFrom(cfg::i2c::kAddrEeprom, static_cast<uint8_t>(chunk));
    if (got != chunk) return Status::kIoError;
    for (uint16_t i = 0; i < chunk; ++i) buffer[done + i] = Wire.read();

    done += chunk;
  }
  return Status::kOk;
}

Status eepromWritePage(uint16_t address, const uint8_t* data, uint16_t length) {
  Wire.beginTransmission(cfg::i2c::kAddrEeprom);
  Wire.write(static_cast<uint8_t>(address >> 8));
  Wire.write(static_cast<uint8_t>(address & 0xFF));
  Wire.write(data, length);
  if (Wire.endTransmission() != 0) return Status::kIoError;
  return eepromWaitWriteComplete();
}

Status eepromWrite(uint16_t address, const uint8_t* data, uint16_t length) {
  if (!eepromPresent) return Status::kNotPresent;
  if (data == nullptr || length == 0) return Status::kInvalidArg;
  if (static_cast<uint32_t>(address) + length > cfg::eeprom::kCapacityBytes) return Status::kInvalidArg;

  uint16_t done = 0;
  while (done < length) {
    const uint16_t addr = address + done;
    // Khong duoc tran qua ranh gioi trang, va khong vuot bo dem I2C.
    const uint16_t roomInPage = cfg::eeprom::kPageSizeBytes - (addr % cfg::eeprom::kPageSizeBytes);
    uint16_t chunk = min<uint16_t>(roomInPage, length - done);
    chunk = min<uint16_t>(chunk, kChunkBytes);

    const Status status = eepromWritePage(addr, data + done, chunk);
    if (!isOk(status)) return status;
    done += chunk;
  }
  return Status::kOk;
}

Status eepromErasePhysical(uint16_t fromAddress, uint16_t length) {
  if (!eepromPresent) return Status::kNotPresent;

  uint8_t blank[cfg::eeprom::kPageSizeBytes];
  memset(blank, 0xFF, sizeof(blank));

  uint16_t done = 0;
  while (done < length) {
    const uint16_t addr       = fromAddress + done;
    const uint16_t roomInPage = cfg::eeprom::kPageSizeBytes - (addr % cfg::eeprom::kPageSizeBytes);
    const uint16_t chunk      = min<uint16_t>(roomInPage, length - done);

    const Status status = eepromWrite(addr, blank, chunk);
    if (!isOk(status)) return status;
    done += chunk;
  }
  return Status::kOk;
}

// ===== 16. BO DEM BAN GHI: 16 BYTE, CRC, VONG TRON =====

// --- Bo cuc byte cua header (16 B) ------------------------------------------
//  0  u32 magic
//  4  u8  version
//  5  u8  reserved
//  6  u16 count
//  8  u16 head
// 10  u16 crc16 (tinh tren 10 byte dau)
// 12  4 B reserved

// --- Bo cuc byte cua ban ghi (16 B) ------------------------------------------
//  0  u32 epoch
//  4  u16 weight  (kg  x 100)
//  6  u16 height  (cm  x 10)
//  8  u8  heartRate
//  9  u8  spo2
// 10  u16 bmi     (     x 100)
// 12  u8  flags
// 13  u8  reserved
// 14  u16 crc16 (tinh tren 14 byte dau)

void put16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
void put32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// Quy doi so thuc <-> so nguyen co dinh dau phay, kem chan tran.
uint16_t toFixed(float value, float factor) {
  if (value <= 0.0f) return 0;
  const float scaled = value * factor;
  return scaled >= 65535.0f ? 65535 : static_cast<uint16_t>(scaled + 0.5f);
}

void serialize(const MeasurementRecord& r, uint8_t* out) {
  memset(out, 0, cfg::eeprom::kRecordSize);
  put32(out + 0,  r.epochSec);
  put16(out + 4,  toFixed(r.weightKg, 100.0f));
  put16(out + 6,  toFixed(r.heightCm, 10.0f));
  out[8]  = r.heartRate;
  out[9]  = r.spo2;
  put16(out + 10, toFixed(r.bmi, 100.0f));
  out[12] = r.flags;
  put16(out + 14, crc16Ccitt(out, kRecCrcCover));
}

Status deserialize(const uint8_t* in, MeasurementRecord& r) {
  if (get16(in + 14) != crc16Ccitt(in, kRecCrcCover)) return Status::kCrcError;

  r.epochSec  = get32(in + 0);
  r.weightKg  = static_cast<float>(get16(in + 4)) / 100.0f;
  r.heightCm  = static_cast<float>(get16(in + 6)) / 10.0f;
  r.heartRate = in[8];
  r.spo2      = in[9];
  r.bmi       = static_cast<float>(get16(in + 10)) / 100.0f;
  r.flags     = in[12];
  return Status::kOk;
}

uint16_t crc16Ccitt(const uint8_t* data, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

uint16_t storeSlotAddress(uint16_t slot) {
  return static_cast<uint16_t>(cfg::eeprom::kRecordAreaStart + slot * cfg::eeprom::kRecordSize);
}

Status storeBegin() {
  storeAvailable = eepromPresent;
  if (!storeAvailable) return Status::kNotPresent;

  const Status status = storeLoadHeader();
  if (!isOk(status)) {
    // Header hong hoac chua tung dung -> khoi tao lai tu dau.
    storeCount = 0;
    storeHead  = 0;
    return storeSaveHeader();
  }
  return Status::kOk;
}

Status storeLoadHeader() {
  uint8_t buf[cfg::eeprom::kHeaderSize];
  const Status status = eepromRead(0, buf, sizeof(buf));
  if (!isOk(status)) return status;

  if (get32(buf + HeaderBytes::kMagicOff) != cfg::eeprom::kMagic) return Status::kCrcError;
  if (buf[HeaderBytes::kVersionOff] != cfg::eeprom::kVersion) return Status::kCrcError;
  if (get16(buf + HeaderBytes::kCrcOff) != crc16Ccitt(buf, HeaderBytes::kCrcCover)) {
    return Status::kCrcError;
  }

  storeCount = get16(buf + HeaderBytes::kCountOff);
  storeHead  = get16(buf + HeaderBytes::kHeadOff);
  if (storeCount > cfg::eeprom::kMaxRecords || storeHead >= cfg::eeprom::kMaxRecords) return Status::kCrcError;
  return Status::kOk;
}

Status storeSaveHeader() {
  uint8_t buf[cfg::eeprom::kHeaderSize];
  memset(buf, 0, sizeof(buf));
  put32(buf + HeaderBytes::kMagicOff, cfg::eeprom::kMagic);
  buf[HeaderBytes::kVersionOff] = cfg::eeprom::kVersion;
  put16(buf + HeaderBytes::kCountOff, storeCount);
  put16(buf + HeaderBytes::kHeadOff, storeHead);
  put16(buf + HeaderBytes::kCrcOff, crc16Ccitt(buf, HeaderBytes::kCrcCover));
  return eepromWrite(0, buf, sizeof(buf));
}

Status storeAppend(const MeasurementRecord& record) {
  if (!storeAvailable) return Status::kNotPresent;

  uint8_t buf[cfg::eeprom::kRecordSize];
  serialize(record, buf);

  const Status status = eepromWrite(storeSlotAddress(storeHead), buf, sizeof(buf));
  if (!isOk(status)) return status;

  storeHead = static_cast<uint16_t>((storeHead + 1) % cfg::eeprom::kMaxRecords);
  if (storeCount < cfg::eeprom::kMaxRecords) ++storeCount;
  return storeSaveHeader();
}

Status storeReadAt(uint16_t index, MeasurementRecord& recordOut) {
  if (!storeAvailable) return Status::kNotPresent;
  if (index >= storeCount) return Status::kEmpty;

  // Ban cu nhat nam ngay sau con tro ghi khi bo dem da tran.
  const uint16_t oldest = static_cast<uint16_t>((storeHead + cfg::eeprom::kMaxRecords - storeCount) % cfg::eeprom::kMaxRecords);
  const uint16_t slot   = static_cast<uint16_t>((oldest + index) % cfg::eeprom::kMaxRecords);

  uint8_t buf[cfg::eeprom::kRecordSize];
  const Status status = eepromRead(storeSlotAddress(slot), buf, sizeof(buf));
  if (!isOk(status)) return status;

  return deserialize(buf, recordOut);
}

Status storeClearLogical() {
  if (!storeAvailable) return Status::kNotPresent;
  storeCount = 0;
  storeHead  = 0;
  return storeSaveHeader();
}

Status storeClearPhysical() {
  if (!storeAvailable) return Status::kNotPresent;
  const Status status = eepromErasePhysical(
      cfg::eeprom::kRecordAreaStart,
      static_cast<uint16_t>(cfg::eeprom::kMaxRecords * cfg::eeprom::kRecordSize));
  if (!isOk(status)) return status;
  return storeClearLogical();
}
