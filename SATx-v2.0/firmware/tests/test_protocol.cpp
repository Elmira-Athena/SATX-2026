// Test giao thuc JSON-lines tren may tinh, khong can phan cung.
#include <cstdio>
#include <cstring>
#include <cmath>

#include "../SATx/SATx.ino"

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, ...)                                        \
  do {                                                          \
    if (cond) { ++g_passed; }                                   \
    else { ++g_failed; printf("  HONG: " __VA_ARGS__); printf("   (%s:%d)\n", __FILE__, __LINE__); } \
  } while (0)

static bool contains(const char* haystack, const char* needle) {
  return strstr(haystack, needle) != nullptr;
}

int main() {
  printf("== Test giao thuc JSON-lines ==\n\n");
  char out[512];
  Command cmd;

  // --- 1. Bo qua dong khong phai JSON -------------------------------------
  // Cong UART0 dung chung voi log bootloader, nen day la duong song con.
  CHECK(!parseCommand("rst:0x1 (POWERON_RESET),boot:0x13", cmd), "bo qua log boot\n");
  CHECK(!parseCommand("", cmd), "bo qua dong rong\n");
  CHECK(!parseCommand("{khong phai json}", cmd), "bo qua JSON hong\n");
  CHECK(!parseCommand("{\"x\":1}", cmd), "bo qua JSON thieu truong t\n");
  CHECK(!parseCommand("{\"t\":\"lenh_la\"}", cmd), "bo qua lenh khong biet\n");
  printf("1. Bo qua dong rac va log bootloader ....... xong\n");

  // --- 2. Cac lenh co ban -------------------------------------------------
  CHECK(parseCommand("{\"t\":\"ping\"}", cmd) && cmd.type == CommandType::kPing, "ping\n");
  CHECK(parseCommand("{\"t\":\"measure\"}", cmd) && cmd.type == CommandType::kMeasure, "measure\n");
  CHECK(parseCommand("{\"t\":\"reset\"}", cmd) && cmd.type == CommandType::kReset, "reset\n");
  CHECK(parseCommand("{\"t\":\"flush\"}", cmd) && cmd.type == CommandType::kFlush, "flush\n");
  printf("2. Lenh co ban ............................. xong\n");

  // --- 3. Dong bo dong ho -------------------------------------------------
  CHECK(parseCommand("{\"t\":\"sync\",\"epoch\":1756713600}", cmd) &&
        cmd.type == CommandType::kSync && cmd.epoch == 1756713600UL,
        "sync epoch sai: %u\n", cmd.epoch);
  printf("3. Dong bo dong ho ......................... xong\n");

  // --- 4. Hieu chuan ------------------------------------------------------
  CHECK(parseCommand("{\"t\":\"cal\",\"target\":\"weight\"}", cmd) &&
        cmd.target == CalibrationTarget::kWeightTare, "cal weight khong co known_kg -> tare\n");
  CHECK(parseCommand("{\"t\":\"cal\",\"target\":\"weight\",\"known_kg\":5.0}", cmd) &&
        cmd.target == CalibrationTarget::kWeightKnown && fabsf(cmd.value - 5.0f) < 0.001f,
        "cal weight co known_kg -> tinh he so (value=%.3f)\n", cmd.value);
  CHECK(parseCommand("{\"t\":\"cal\",\"target\":\"height\",\"mount_cm\":198.5}", cmd) &&
        cmd.target == CalibrationTarget::kHeightMount && fabsf(cmd.value - 198.5f) < 0.01f,
        "cal height (value=%.3f)\n", cmd.value);
  printf("4. Lenh hieu chuan ......................... xong\n");

  // --- 5. Cau hinh WiFi ---------------------------------------------------
  CHECK(parseCommand("{\"t\":\"cfg\",\"wifi\":{\"ssid\":\"PhongLab\",\"pass\":\"bimat123\"}}", cmd) &&
        cmd.type == CommandType::kConfig &&
        strcmp(cmd.ssid, "PhongLab") == 0 && strcmp(cmd.password, "bimat123") == 0,
        "cfg wifi: ssid='%s' pass='%s'\n", cmd.ssid, cmd.password);
  printf("5. Cau hinh WiFi ........................... xong\n");

  // --- 6. Xoa kho ---------------------------------------------------------
  CHECK(parseCommand("{\"t\":\"erase\"}", cmd) && !cmd.physicalErase, "erase mac dinh la logic\n");
  CHECK(parseCommand("{\"t\":\"erase\",\"physical\":true}", cmd) && cmd.physicalErase,
        "erase physical\n");
  printf("6. Lenh xoa kho ............................ xong\n");

  // --- 7. Dong goi ket qua day du -----------------------------------------
  MeasurementRecord r;
  r.epochSec  = 1756713612;
  r.weightKg  = 62.40f;
  r.heightCm  = 171.2f;
  r.heartRate = 78;
  r.spo2      = 97;
  r.set(flag::kTimeValid, true);
  r.set(flag::kWeightValid, true);
  r.set(flag::kHeightValid, true);
  r.set(flag::kHeartValid, true);
  r.set(flag::kSpo2Valid, true);
  r.computeBmi();

  size_t n = encodeResult(out, sizeof(out), r, RecordSource::kLive);
  CHECK(n > 0 && out[n - 1] == '\n', "ban tin phai ket thuc bang xuong dong\n");
  CHECK(contains(out, "\"t\":\"result\""), "thieu truong t\n");
  CHECK(contains(out, "\"w\":62.40"), "can nang sai: %s", out);
  CHECK(contains(out, "\"h\":171.2"), "chieu cao sai: %s", out);
  CHECK(contains(out, "\"hr\":78"), "nhip tim sai\n");
  CHECK(contains(out, "\"spo2\":97"), "spo2 sai\n");
  CHECK(contains(out, "\"ok\":true"), "phai danh dau day du\n");
  CHECK(contains(out, "\"src\":\"live\""), "nguon phai la live\n");
  printf("7. Dong goi ket qua day du ................. xong\n");

  // --- 8. Truong thieu phai la null, khong phai so 0 gia ------------------
  MeasurementRecord partial;
  partial.weightKg = 70.0f;
  partial.set(flag::kWeightValid, true);
  n = encodeResult(out, sizeof(out), partial, RecordSource::kEeprom);
  CHECK(n > 0, "dong goi ban ghi thieu truong\n");
  CHECK(contains(out, "\"hr\":null"), "nhip tim khong doc duoc phai la null, khong phai 0: %s", out);
  CHECK(contains(out, "\"spo2\":null"), "spo2 khong doc duoc phai la null\n");
  CHECK(contains(out, "\"h\":null"), "chieu cao khong doc duoc phai la null\n");
  CHECK(contains(out, "\"w\":70.00"), "can nang doc duoc van phai co gia tri\n");
  CHECK(contains(out, "\"ok\":false"), "ban ghi thieu truong khong duoc danh dau day du\n");
  CHECK(contains(out, "\"src\":\"eeprom\""), "nguon phai la eeprom\n");
  CHECK(contains(out, "\"tsValid\":false"), "chua dong bo gio thi tsValid phai la false\n");
  printf("8. Truong thieu tra ve null (khong phai 0) . xong\n");

  // --- 9. Cac ban tin con lai ---------------------------------------------
  CHECK(encodePong(out, sizeof(out), "uart", 12345, 3) > 0 &&
        contains(out, "\"pending\":3"), "pong\n");
  CHECK(encodeState(out, sizeof(out), "measuring", "ppg", 40, 9) > 0 &&
        contains(out, "\"pct\":40") && contains(out, "\"eta\":9"), "state\n");
  CHECK(encodeError(out, sizeof(out), "NO_WEIGHT", "loi") > 0 &&
        contains(out, "\"code\":\"NO_WEIGHT\""), "error\n");
  CHECK(encodeInfo(out, sizeof(out), "uart", "192.168.1.63", 0, 200.0f, 2852.35f) > 0 &&
        contains(out, "\"ip\":\"192.168.1.63\""), "info\n");
  printf("9. Pong / state / error / info ............. xong\n");

  // --- 10. Ban tin tu kiem tra --------------------------------------------
  CHECK(encodeSelfTest(out, sizeof(out), true, true, true) > 0 &&
        contains(out, "\"lcd\":true") && contains(out, "\"ppg\":true") &&
        contains(out, "\"eeprom\":true"), "selftest du thiet bi\n");
  CHECK(encodeSelfTest(out, sizeof(out), true, false, true) > 0 &&
        contains(out, "\"ppg\":false"), "selftest thieu cam bien nhip tim: %s", out);
  printf("10. Ban tin tu kiem tra .................... xong\n");

  // --- 11. Bo dem qua nho thi tra 0, khong tran ---------------------------
  char tiny[16];
  CHECK(encodeResult(tiny, sizeof(tiny), r, RecordSource::kLive) == 0,
        "bo dem qua nho phai tra 0\n");
  printf("11. Bo dem qua nho tra 0 ................... xong\n");

  printf("\n== Ket qua: %d dat, %d hong ==\n", g_passed, g_failed);
  return g_failed == 0 ? 0 : 1;
}
