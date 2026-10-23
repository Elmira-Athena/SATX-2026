// Chay nguyen sketch voi HAL gia lap; khong can chen nhanh test vao firmware.
#include "../SATx/SATx.ino"

static int passed = 0, failed = 0;
#define CHECK(condition, message) do { \
  if (condition) ++passed; \
  else { ++failed; printf("HONG: %s (dong %d)\n", message, __LINE__); } \
} while (0)

static void tick(uint32_t now) { hal::nowMs = now; loop(); }

int main() {
  Wire.reset();
  setup();
  CHECK(Serial.outgoing.find("\"t\":\"selftest\"") != std::string::npos,
        "boot phai gui selftest truoc khi co ket noi");
  tick(1499);
  CHECK(state == AppState::kSelfTest, "cam bien can cho du 1500 ms");
  tick(1500);
  CHECK(state == AppState::kIdle, "het selftest phai san sang");

  // Nut START thuc: loc doi, PPG -> chieu cao -> can -> luu khi offline.
  hal::gpio[cfg::pin::kButtonStart] = LOW;
  tick(2000);
  CHECK(state == AppState::kIdle, "khong nhan nut truoc khi het doi");
  tick(2030);
  CHECK(state == AppState::kMeasurePpg, "START bat dau do PPG");
  hal::gpio[cfg::pin::kButtonStart] = HIGH;
  clockSync(1756713600, 2030);
#if SATX_SIMULATE_SENSORS
  tick(4030);
  tick(14030);
#else
  hal::hxRaw = 177986; // 62.40 kg voi he so mac dinh 2852.35.
  for (unsigned i = 0; i < 100; ++i) g_sensor.samples.emplace_back(50000, 60000);
  tick(2060);
  CHECK(pulsePhase == PulsePhase::kPaused, "PPG phai cho ngon tay on dinh");
  tick(2360);
  for (unsigned i = 0; i < 100; ++i) g_sensor.samples.emplace_back(50000, 60000);
  tick(4360);
  CHECK(pulsePhase == PulsePhase::kSampling, "PPG chi dem sau warmup");
  tick(14360);
#endif
  CHECK(state == AppState::kMeasureHeight, "PPG xong phai do chieu cao");
  loop();
  CHECK(state == AppState::kMeasureWeight, "chieu cao xong phai do can nang");
  loop();
  CHECK(state == AppState::kDone && record.isComplete(), "phep do phai hoan thanh du 4 chi so");
  CHECK(storeCount == 1, "mat ca hai kenh phai luu ket qua vao EEPROM");
  MeasurementRecord stored;
  CHECK(isOk(storeReadAt(0, stored)) && stored.isComplete() && stored.has(flag::kTimeValid),
        "ket qua offline phai giu cac co va thoi gian");
#if !SATX_SIMULATE_SENSORS
  CHECK(fabsf(stored.weightKg - 62.4f) < 0.01f && fabsf(stored.heightCm - 15.0f) < 0.1f,
        "he so can va quy doi sieu am phai giu nguyen");
#endif
  Command reset;
  reset.type = CommandType::kReset;
  handleCommand(reset);
  CHECK(state == AppState::kIdle && storeCount == 1, "RESET giu nguyen du lieu dang cho");

  // Khi UART tro lai, tu day ban ghi cu va chi xoa sau khi truyen xong.
  Serial.outgoing.clear();
  Serial.receive("rst:0x1\n{\"t\":\"ping\"}\n");
  const uint32_t reconnectedAt = millis() + 10;
  tick(reconnectedAt);
  CHECK(linkState == LinkState::kUartActive && storeCount == 1,
        "ping kich hoat UART va chuan bi day du lieu");
  tick(reconnectedAt + 40);
  CHECK(Serial.outgoing.find("\"src\":\"eeprom\"") != std::string::npos && storeCount == 1,
        "gui ban ghi EEPROM truoc khi xoa header");
  tick(reconnectedAt + 80);
  CHECK(storeCount == 0, "day het moi xoa logic EEPROM");

  // UART duoc uu tien; WiFi chi thay the sau timeout va tra lai khi do xong.
  Serial.receive("{\"t\":\"cfg\",\"wifi\":{\"ssid\":\"Lab\",\"pass\":\"secret\"}}\n");
  tick(reconnectedAt + 100);
  g_server.receive(WStype_CONNECTED);
  g_server.receive(WStype_TEXT, "{\"t\":\"ping\"}");
  linkUpdate(reconnectedAt + 3099);
  CHECK(linkState == LinkState::kUartActive, "UART con song o moc timeout - 1 ms");
  linkUpdate(reconnectedAt + 3100);
  CHECK(linkState == LinkState::kWifiActive && !WiFi.sleeping,
        "UART timeout phai danh thuc va chuyen qua WiFi");
  enterMeasuring(reconnectedAt + 3100);
  Serial.receive("{\"t\":\"ping\"}\n");
  linkUpdate(reconnectedAt + 3101);
  CHECK(linkState == LinkState::kWifiActive, "UART tro lai giua phep do phai cho");
  handleLine(true, "{\"t\":\"reset\"}", reconnectedAt + 3101);
  CHECK(state == AppState::kMeasurePpg, "kenh khong hoat dong khong duoc dieu khien");
  enterIdle(reconnectedAt + 3102);
  linkUpdate(reconnectedAt + 3102);
  CHECK(linkState == LinkState::kUartActive && WiFi.sleeping,
        "het phep do phai tra ve UART va cho WiFi ngu");

  // Dong qua dai phai bi bo tron ven, dong tiep theo van duoc nhan.
  Serial.receive((std::string(600, 'x') + "{\"t\":\"reset\"}\n").c_str());
  enterMeasuring(reconnectedAt + 3110);
  linkUpdate(reconnectedAt + 3110);
  CHECK(state == AppState::kMeasurePpg, "khong xu ly phan duoi dong tran");
  Serial.receive("{\"t\":\"reset\"}\r\n");
  linkUpdate(reconnectedAt + 3120);
  CHECK(state == AppState::kIdle, "dong hop le sau dong tran van hoat dong");

  // Doc dung du lieu NVS cua firmware cu, va luu lai cung cac key do.
  g_prefs.floats = {{"w_off", 123.25f}, {"w_scale", 3000.0f}, {"h_mount", 198.5f}};
  g_prefs.strings = {{"wifi_ssid", "OldLab"}, {"wifi_pass", "old-secret"}};
  settingsBegin();
  CHECK(g_prefs.nameSpace == "satx" && settingsWeightOffset == 123.25f &&
        settingsWeightScale == 3000.0f && settingsMountHeightCm == 198.5f,
        "doc hieu chuan NVS tu namespace/key cu");
  CHECK(strcmp(settingsSsid, "OldLab") == 0 && strcmp(settingsPassword, "old-secret") == 0,
        "doc lai thong tin WiFi da luu");
  settingsSetWeightCalibration(10.0f, 2800.0f);
  settingsSetMountHeightCm(201.5f);
  settingsSetWifiCredentials("NewLab", "new-secret");
  settingsWeightOffset = settingsWeightScale = settingsMountHeightCm = 0;
  settingsSsid[0] = settingsPassword[0] = '\0';
  settingsBegin();
  CHECK(settingsWeightOffset == 10.0f && settingsWeightScale == 2800.0f &&
        settingsMountHeightCm == 201.5f && strcmp(settingsSsid, "NewLab") == 0 &&
        strcmp(settingsPassword, "new-secret") == 0,
        "hieu chuan va WiFi moi phai song sot qua khoi dong lai");

#if !SATX_SIMULATE_SENSORS
  // Roi tay dung dong ho, dat lai phai warmup, tong thoi gian do van duoc giu.
  pulseStart(100000);
  for (unsigned i = 0; i < 100; ++i) g_sensor.samples.emplace_back(50000, 60000);
  pulseUpdate(100001);
  pulseUpdate(100301);
  for (unsigned i = 0; i < 100; ++i) g_sensor.samples.emplace_back(50000, 60000);
  pulseUpdate(102301);
  pulseUpdate(106301);
  CHECK(pulseProgressPercent(106301) == 40, "PPG dem dung thoi gian truoc khi roi tay");
  for (unsigned i = 0; i < 100; ++i) g_sensor.samples.emplace_back(0, 0);
  pulseUpdate(106302);
  pulseUpdate(106602);
  CHECK(pulsePhase == PulsePhase::kPaused && pulseProgressPercent(120000) == 43,
        "thoi gian tam dung khong duoc tinh vao cua so 10 giay");
  for (unsigned i = 0; i < 100; ++i) g_sensor.samples.emplace_back(50000, 60000);
  pulseUpdate(120000);
  pulseUpdate(120300);
  CHECK(pulsePhase == PulsePhase::kWarmup, "dat lai tay phai warmup lai");
  for (unsigned i = 0; i < 100; ++i) g_sensor.samples.emplace_back(50000, 60000);
  pulseUpdate(122300);
  pulseUpdate(127998);
  CHECK(pulsePhase == PulsePhase::kSampling, "PPG chua xong khi moi du 9999 ms");
  pulseUpdate(128000);
  CHECK(pulsePhase == PulsePhase::kFinished, "PPG cong don thoi gian do qua lan tam dung");
#endif

  // Qua 60 s khong co ngon tay phai bao loi, sau 3 s tu ve san sang.
  hal::nowMs = reconnectedAt + 4000;
  enterMeasuring(millis());
#if !SATX_SIMULATE_SENSORS
  pulseFilled = 0;
  tick(stateEnteredMs + 60001);
  CHECK(state == AppState::kError && lastError == Status::kTimeout,
        "mat ngon tay khong duoc treo mai");
  tick(stateEnteredMs + 3000);
  CHECK(state == AppState::kIdle, "loi phai tu dua ve san sang");
#endif

  clockSync(1000, 0xfffffff0U);
  CHECK(clockNowEpoch(984) == 1001, "dong ho phai dung qua vong tran millis");
  clockSync(0, 0);
  CHECK(!clockValid && clockNowEpoch(1234) == 0, "chua dong bo thi khong gia thoi gian");
#if !SATX_SIMULATE_SENSORS
  // Xung LOW qua PC817: 2900 us = 50 cm, cao lap 200 cm -> nguoi cao 150 cm.
  heightMountHeightCm = 200.0f;
  hal::echoUs = 2900;
  float measuredHeight = 0.0f;
  CHECK(isOk(heightMeasure(measuredHeight)) && fabsf(measuredHeight - 150.0f) < 0.1f,
        "ECHO dao qua PC817 phai do duoc chieu cao 150 cm");
  record = MeasurementRecord{};
  stepHeight(millis());
  char resultJson[512];
  encodeResult(resultJson, sizeof(resultJson), record, RecordSource::kLive);
  CHECK(record.has(flag::kHeightValid) && std::string(resultJson).find("\"h\":150.0") != std::string::npos,
        "so do chieu cao hop le phai duoc gui cho web");

  hal::echoUs = 0;
  record = MeasurementRecord{};
  record.weightKg = 62.4f;
  record.set(flag::kWeightValid, true);
  stepHeight(millis());
  encodeResult(resultJson, sizeof(resultJson), record, RecordSource::kLive);
  CHECK(!record.has(flag::kHeightValid) && record.has(flag::kWeightValid) &&
        std::string(resultJson).find("\"h\":null") != std::string::npos,
        "mat ECHO phai gui h null va giu chi so khac");

  hal::echoUs = 17400; // 300 cm, lon hon cao lap 200 cm.
  CHECK(heightMeasure(measuredHeight) == Status::kOutOfRange,
        "khoang cach vuot cao lap khong duoc thanh chieu cao hop le");

  // Mo hinh that: cam bien cach mat can 20 cm, do vat nho thay vi nguoi.
  g_prefs.floats.erase("h_mount");
  settingsBegin();
  heightMountHeightCm = settingsMountHeightCm;
  hal::echoUs = 1160; // Mat can trong: khoang cach 20 cm.
  CHECK(isOk(heightMeasure(measuredHeight)) && fabsf(measuredHeight) < 0.01f,
        "mo hinh mac dinh 20 cm phai cho chieu cao 0 khi mat can trong");

  heightMountHeightCm = 20.0f;
  hal::echoUs = 870; // Khoang cach 15 cm -> vat cao 5 cm.
  CHECK(isOk(heightMeasure(measuredHeight)) && fabsf(measuredHeight - 5.0f) < 0.01f,
        "vat cao 5 cm khong duoc bi loai boi nguong do nguoi 50 cm");

  hal::echoUs = 1148; // Khoang cach 19.79 cm -> chieu cao gan 0.2 cm.
  CHECK(isOk(heightMeasure(measuredHeight)) && fabsf(measuredHeight - 0.2f) < 0.02f,
        "mat can cach gan 20 cm khong duoc hien chieu cao 180.2 cm");

  // Giu tung xung theo thu tu doc de chan doan dao dong cua vat nho tren web.
  linkState = LinkState::kUartActive;
  Serial.outgoing.clear();
  hal::echoSequence = {974, 1038, 1032, 1150, 1148};
  CHECK(isOk(heightMeasure(measuredHeight)) && fabsf(measuredHeight - 2.10345f) < 0.001f,
        "nhat ky khong duoc thay doi trung vi va tu bu sai so chieu cao");
  JsonDocument diagnostic;
  CHECK(deserializeJson(diagnostic, Serial.outgoing) == DeserializationError::Ok &&
        diagnostic["t"] == "log" && diagnostic["mount_cm"] == 20.0f &&
        diagnostic["valid"] == 5 && diagnostic["echo_us"][0] == 974 &&
        diagnostic["echo_us"][1] == 1038 && diagnostic["echo_us"][2] == 1032 &&
        diagnostic["echo_us"][3] == 1150 && diagnostic["echo_us"][4] == 1148,
        "web phai nhan du xung ECHO goc theo thu tu va cao lap");

  Serial.outgoing.clear();
  hal::echoSequence = {0, 1038, 0, 0, 1032};
  const Status diagnosticStatus = heightMeasure(measuredHeight);
  CHECK(diagnosticStatus == Status::kNoisy &&
        deserializeJson(diagnostic, Serial.outgoing) == DeserializationError::Ok &&
        diagnostic["valid"] == 2 && diagnostic["echo_us"][0] == 0 &&
        diagnostic["echo_us"][4] == 1032,
        "van phai gui nhat ky khi khong du mau, giu timeout la 0 us");

  // Mo hinh can vat nho: 200 g va mat can trong van la so do hop le.
  weightSetCalibration(1000.0f, 10000.0f);
  hal::hxReady = true;
  hal::hxRaw = 3000;
  float measuredWeight = -1.0f;
  CHECK(isOk(weightMeasure(measuredWeight)) && fabsf(measuredWeight - 0.2f) < 0.001f,
        "vat nhe 200 g khong duoc bi loai boi nguong do nguoi 2 kg");
  hal::hxRaw = 1000;
  CHECK(isOk(weightMeasure(measuredWeight)) && measuredWeight == 0.0f,
        "mat can trong da tru bi phai hien 0 kg");
  hal::hxRaw = 0;
  CHECK(weightMeasure(measuredWeight) == Status::kOutOfRange,
        "sua can vat nho van phai loai can nang am");
  hal::hxRaw = 2511000;
  CHECK(weightMeasure(measuredWeight) == Status::kOutOfRange,
        "van phai loai can nang vuot 250 kg");
  hal::hxReady = false;
  CHECK(weightMeasure(measuredWeight) == Status::kTimeout,
        "HX711 khong tra loi khong duoc gia thanh so 0 kg");
  hal::hxReady = true;
#endif
  // Lenh WiFi duoc xu ly giua vong lap; millis co the tang truoc khi bat dau do.
  for (const auto timing : {std::pair<uint32_t, uint32_t>{1000, 1005},
                            std::pair<uint32_t, uint32_t>{0xfffffffeU, 3}}) {
    hal::nowMs = timing.first;
    enterIdle(millis());
    uartSeenPeer = false;
    wifiStarted = true;
    wifiClientCount = 1;
    wifiSeenPeer = true;
    wifiLastPeerMs = millis();
    linkState = LinkState::kWifiActive;
    g_server.outgoing.clear();
    g_server.pendingEvent = [timing] {
      hal::nowMs = timing.second;
      g_server.receive(WStype_TEXT, "{\"t\":\"measure\"}");
    };
    loop();
    CHECK(state == AppState::kMeasurePpg && stateEnteredMs == timing.second &&
          g_server.outgoing.find("\"code\":\"TIMEOUT\"") == std::string::npos,
          "lenh do WiFi khong duoc TIMEOUT ngay, ke ca khi millis tran");
  }
  printf("Workflow (simulate=%d): %d dat, %d hong\n", SATX_SIMULATE_SENSORS, passed, failed);
  return failed ? 1 : 0;
}
