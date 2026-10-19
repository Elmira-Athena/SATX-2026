#pragma once
#include <cstdint>
// Test firmware; thuat toan Maxim ben ngoai duoc thay bang ket qua co dinh.
inline void maxim_heart_rate_and_oxygen_saturation(uint32_t*, int32_t, uint32_t*,
                                                  int32_t* spo2, int8_t* spo2Valid,
                                                  int32_t* heartRate, int8_t* heartValid) {
  *spo2 = 97; *spo2Valid = 1; *heartRate = 78; *heartValid = 1;
}
