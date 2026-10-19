// Test RecordStore tren may tinh, khong can phan cung.
#include <cstdio>
#include <cmath>
#include <cstring>

#include "../SATx/SATx.ino"

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, ...)                                        \
  do {                                                          \
    if (cond) { ++g_passed; }                                   \
    else { ++g_failed; printf("  HONG: " __VA_ARGS__); printf("   (%s:%d)\n", __FILE__, __LINE__); } \
  } while (0)

static MeasurementRecord makeRecord(uint32_t epoch, float kg, float cm, uint8_t hr, uint8_t sp) {
  MeasurementRecord r;
  r.epochSec  = epoch;
  r.weightKg  = kg;
  r.heightCm  = cm;
  r.heartRate = hr;
  r.spo2      = sp;
  r.set(flag::kTimeValid, true);
  r.set(flag::kWeightValid, true);
  r.set(flag::kHeightValid, true);
  r.set(flag::kHeartValid, true);
  r.set(flag::kSpo2Valid, true);
  r.computeBmi();
  return r;
}

static bool nearly(float a, float b, float tol) { return fabsf(a - b) <= tol; }

int main() {
  printf("== Test RecordStore / Eeprom24C ==\n\n");


  // --- 1. Khoi tao tren EEPROM trong ---------------------------------------
  Wire.reset();
  eepromBegin();
  CHECK(isOk(storeBegin()), "begin() tren EEPROM trong phai thanh cong\n");
  CHECK(storeCount == 0, "EEPROM moi phai rong, dang co %u\n", storeCount);
  printf("1. Khoi tao tren EEPROM trong .............. xong\n");

  // --- 2. Ghi/doc mot ban ghi, kiem tra do chinh xac quy doi ---------------
  const MeasurementRecord a = makeRecord(1756713612, 62.40f, 171.2f, 78, 97);
  CHECK(isOk(storeAppend(a)), "append() phai thanh cong\n");
  CHECK(storeCount == 1, "sau 1 append phai co 1 ban ghi\n");

  MeasurementRecord back;
  CHECK(isOk(storeReadAt(0, back)), "readAt(0) phai thanh cong\n");
  CHECK(back.epochSec == a.epochSec, "epoch sai: %u != %u\n", back.epochSec, a.epochSec);
  CHECK(nearly(back.weightKg, 62.40f, 0.005f), "can nang sai: %.3f\n", back.weightKg);
  CHECK(nearly(back.heightCm, 171.2f, 0.05f), "chieu cao sai: %.3f\n", back.heightCm);
  CHECK(back.heartRate == 78, "nhip tim sai: %u\n", back.heartRate);
  CHECK(back.spo2 == 97, "spo2 sai: %u\n", back.spo2);
  CHECK(nearly(back.bmi, a.bmi, 0.005f), "bmi sai: %.3f != %.3f\n", back.bmi, a.bmi);
  CHECK(back.flags == a.flags, "co bao sai: %02X != %02X\n", back.flags, a.flags);
  // Fixture doc lap: bat loi doi bo cuc byte/CRC du serialize va deserialize cung doi.
  const uint8_t expectedBytes[16] = {
    0x8c, 0x52, 0xb5, 0x68, 0x60, 0x18, 0xb0, 0x06,
    0x4e, 0x61, 0x51, 0x08, 0x1f, 0x00, 0x9a, 0x54
  };
  CHECK(memcmp(Wire.memory.data() + 16, expectedBytes, 16) == 0,
        "bo cuc ban ghi cu phai giu nguyen tung byte\n");
  printf("2. Ghi/doc mot ban ghi ..................... xong (BMI %.2f)\n", back.bmi);

  // --- 3. Nhieu ban ghi, dung thu tu cu -> moi ----------------------------
  Wire.reset();
  storeBegin();
  for (int i = 0; i < 100; ++i) {
    CHECK(isOk(storeAppend(makeRecord(1000 + i, 50.0f + i * 0.1f, 160.0f, 70 + i % 30, 95))),
          "append thu %d that bai\n", i);
  }
  CHECK(storeCount == 100, "phai co 100 ban ghi, dang co %u\n", storeCount);
  bool ordered = true;
  for (int i = 0; i < 100; ++i) {
    MeasurementRecord r;
    if (!isOk(storeReadAt(i, r)) || r.epochSec != uint32_t(1000 + i)) { ordered = false; break; }
  }
  CHECK(ordered, "readAt phai tra ve thu tu cu nhat -> moi nhat\n");
  printf("3. 100 ban ghi, dung thu tu ................ xong\n");

  // --- 4. Vong tron: ghi qua suc chua -------------------------------------
  Wire.reset();
  storeBegin();
  const uint16_t maxRecords = cfg::eeprom::kMaxRecords;
  const uint32_t total = uint32_t(maxRecords) + 25;   // tran 25 ban
  for (uint32_t i = 0; i < total; ++i) {
    storeAppend(makeRecord(50000 + i, 60.0f, 170.0f, 75, 96));
  }
  CHECK(storeCount == maxRecords, "khi tran phai giu dung %u ban, dang co %u\n",
        maxRecords, storeCount);
  MeasurementRecord oldest, newest;
  CHECK(isOk(storeReadAt(0, oldest)), "doc ban cu nhat sau khi tran\n");
  CHECK(isOk(storeReadAt(maxRecords - 1, newest)), "doc ban moi nhat sau khi tran\n");
  CHECK(oldest.epochSec == 50000 + 25, "ban cu nhat phai la %u, dang la %u\n",
        50000 + 25, oldest.epochSec);
  CHECK(newest.epochSec == 50000 + total - 1, "ban moi nhat phai la %u, dang la %u\n",
        50000 + total - 1, newest.epochSec);
  printf("4. Vong tron khi tran (%u ban) ............ xong\n", maxRecords);

  // --- 5. Xoa logic -------------------------------------------------------
  CHECK(isOk(storeClearLogical()), "clearLogical() phai thanh cong\n");
  CHECK((storeCount == 0), "sau khi xoa phai rong\n");
  CHECK(storeReadAt(0, back) == Status::kEmpty, "doc tu kho rong phai tra kEmpty\n");
  printf("5. Xoa logic ............................... xong\n");

  // --- 6. Header ben vung qua lan khoi dong lai --------------------------
  storeBegin();
  storeAppend(makeRecord(777, 55.5f, 165.0f, 80, 98));
  storeAppend(makeRecord(888, 66.6f, 175.0f, 60, 99));
  storeCount = 0;  // Gia lap mat RAM khi khoi dong lai.
  CHECK(isOk(storeBegin()), "mo lai kho phai thanh cong\n");
  CHECK(storeCount == 2, "sau khi mo lai phai con 2 ban, dang co %u\n", storeCount);
  MeasurementRecord r0;
  CHECK(isOk(storeReadAt(0, r0)) && r0.epochSec == 777, "du lieu phai con nguyen sau khi mo lai\n");
  printf("6. Du lieu song sot qua khoi dong lai ...... xong\n");

  // --- 7. Phat hien du lieu hong bang CRC --------------------------------
  uint8_t* mem = Wire.memory.data();
  mem[cfg::eeprom::kRecordAreaStart + 5] ^= 0xFF;   // lam hong 1 byte cua ban ghi dau
  MeasurementRecord corrupted;
  CHECK(storeReadAt(0, corrupted) == Status::kCrcError,
        "ban ghi bi sua phai bi CRC bat duoc\n");
  printf("7. CRC bat duoc ban ghi hong ............... xong\n");

  // --- 8. Header hong -> tu khoi tao lai, khong treo ---------------------
  mem[2] ^= 0xFF;
  CHECK(isOk(storeBegin()), "header hong phai duoc khoi tao lai\n");
  CHECK(storeCount == 0, "sau khi khoi tao lai phai rong\n");
  CHECK(isOk(storeAppend(makeRecord(999, 70.0f, 180.0f, 65, 97))),
        "van ghi duoc sau khi phuc hoi\n");
  printf("8. Phuc hoi khi header hong ................ xong\n");

  // Driver that phai chia ghi thanh cac trang I2C hop le, ke ca khi xoa vat ly.
  CHECK(isOk(storeClearPhysical()) && storeCount == 0, "xoa vat ly phai lam rong kho\n");
  bool blank = true;
  for (size_t i = 16; i < Wire.memory.size(); ++i) blank = blank && Wire.memory[i] == 0xFF;
  CHECK(blank, "xoa vat ly phai xoa toan bo vung ban ghi\n");
  CHECK(Wire.pageViolations == 0, "driver khong duoc ghi tran trang EEPROM\n");

  printf("\n== Ket qua: %d dat, %d hong ==\n", g_passed, g_failed);
  return g_failed == 0 ? 0 : 1;
}
