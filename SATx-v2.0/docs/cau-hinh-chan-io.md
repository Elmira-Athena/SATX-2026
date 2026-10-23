# Cấu hình chân IO

Đối chiếu trực tiếp trang 1 của [Cấu hình chân IO.pdf](<../requirememt/Mục yêu cầu SATx/Cấu hình chân  IO.pdf>)
(`Sche_Satx.SchDoc`, Altium).
Board: **ESP32 DevKit V1 (WROOM-32)**.
U1 trên PDF là symbol **38 chân**. Cần đối chiếu board thật và footprint/socket; tên
"DevKit V1" không đủ để suy rằng mọi biến thể có cùng số chân và thứ tự chân.

Chân IO, ngưỡng và mốc thời gian của ứng dụng nằm trong phần `namespace cfg` ở đầu
[`firmware/SATx/SATx.ino`](../firmware/SATx/SATx.ino). Khi đổi phần cứng, sửa tại đây.
Hai sketch chẩn đoán `LcdLedTest` và `LedBlinkTest` khai báo riêng các hằng số cần dùng để
chạy độc lập; các giá trị này phải khớp cấu hình của ứng dụng.

**Sơ đồ PDF có lỗi nối bus I2C vào SD1/SD0 của U1 — xem HW-6 trước khi lắp ESP32 lại.**
Bảng dưới là chân GPIO mà firmware yêu cầu, không xác nhận rằng PCB đã đi dây đúng như vậy.

## Bảng chân

| Khối | Net trên schematic | GPIO | Hướng | Ghi chú |
|---|---|---|---|---|
| I2C dùng chung | `SDA_LCD` / `SDA_M3` / `SDA_EE` | **21** | vào–ra | Chân IO21 thật; xem HW-6, kiểm tra pull-up trên module |
| | `SCL_LCD` / `SCL_M3` / `SCL_EE` | **22** | ra | Chân IO22 thật; xem HW-6, kiểm tra pull-up trên module |
| HC-SR04 | `TRIG` | **5** | ra | xung kích 10 µs |
| | `ECHO` | **18** | vào | qua PC817 + R 220 Ω — xem **HW-1** |
| HX711 | `DAT` | **34** | vào | GPIO34 chỉ vào, đúng chuẩn cho DOUT |
| | `CLK` | **19** | ra | |
| LED đỏ (đang đo) | — | **33** | ra | R 330 Ω |
| LED xanh (đo xong) | — | **32** | ra | R 330 Ω |
| Nút Start | `H3` | **26** | vào | nối xuống GND, dùng pull-up nội, tích cực mức thấp |
| Nút Reset | `H4` | **25** | vào | như trên |
| UART ↔ CH340 | `TX_E` = `TXD0` | **1** | ra | 115200 8N1 — xem **HW-2** |
| | `RX_E` = `RXD0` | **3** | vào | |

Địa chỉ trên bus I2C: LCD `0x27` (hoặc `0x3F`), MAX30102 `0x57`, EEPROM AT24C256 `0x50`.

Nguồn theo PDF: adapter → đường `+5` → M1 LM2596 → đường `3.3V` → **3V3 của U1**.
Không phải VIN/EXT_5V: chân EXT_5V của U1 đang đánh dấu không nối.
R4/R5 4k7 trong PDF là pull-up cho **nút Start/Reset**, không phải pull-up I2C.
Hai LED dùng chung điện trở R3 330 Ω xuống GND theo sơ đồ.

## Cảnh báo phần cứng

Các điểm dưới đây cần kiểm chứng khi lắp ráp. Đặc biệt, lỗi nguồn có thể chặn cả khởi động
và nạp firmware, trước khi hệ thống đọc được cảm biến.

### HW-1 — PC817 trên đường ECHO đảo mức

Theo PDF, PC817 có emitter (chân 3) nối GND, collector (chân 4) nối IO18 và được R1 4k7
kéo lên 3,3 V. Cách mắc cực phát chung này **đảo** tín hiệu: khi HC-SR04 kéo ECHO lên cao
thì chân IO18 xuống thấp. Firmware phải đo xung **LOW** nếu PCB giữ cách mắc này.

Firmware để cực tính thành hằng số:

```cpp
// firmware/SATx/SATx.ino
constexpr int kEchoActiveLevel = 0;   // LOW qua PC817; HIGH = 1 neu mach khong dao
```

Bài test `T-HCSR04` trong [bring-up-checklist.md](bring-up-checklist.md) phải xác minh giá trị
này **trước** khi tin bất kỳ số đo chiều cao nào, nhất là khi đã sửa phần ECHO trên PCB.
Chọn sai cực tính có thể làm `pulseIn()` hết thời gian chờ hoặc đo phần nghỉ thay vì xung
phản hồi; firmware sẽ bỏ số đo lỗi và gửi `h: null`, khiến web hiển thị `--`.

Datasheet PC817 ghi thời gian sườn tối đa 18 µs ở điều kiện thử `RL = 100 Ω`,
`VCE = 2 V`, `IC = 2 mA`. PCB dùng pull-up R1 **4k7**, nên không thể dùng con số đó để
bảo đảm sai số khoảng cách trên mạch này. Nếu xung bị kéo dài qua opto, cần so dạng xung
trước/sau PC817 để xác nhận; không suy ra hệ số bù chỉ từ thông số datasheet.
Nguồn: [datasheet PC817 của Sharp](https://global.sharp/products/device/lineup/data/pdf/datasheet/PC817XxNSZ1B_e.pdf).

### HW-2 — Có thể xung đột UART0

DevKit V1 **đã có sẵn** một CH340 nối vào GPIO1/GPIO3. Schematic lại thêm connector `P3` cũng
nối vào đúng hai chân đó. Cắm cả hai cáp cùng lúc là hai bộ đệm cùng lái một đường truyền —
dữ liệu sẽ hỏng ngẫu nhiên và rất khó truy nguyên.

Hai cách xử lý:

1. **Chỉ cắm một cáp tại một thời điểm.** Giữ nguyên mạch, không đổi gì.
2. **Chuyển P3 sang UART2** (khuyến nghị): `TX = IO17`, `RX = IO16` — hai chân này đang để trống
   trên schematic. Firmware đã hỗ trợ sẵn, chỉ cần đổi một dòng:

   ```cpp
   #define SATX_LINK_UART_NUM 2   // 0 = UART0 như bản vẽ, 2 = UART2
   ```

Lưu ý phụ: ROM bootloader của ESP32 in log ra UART0 lúc khởi động. Đó là lý do cả firmware lẫn
giao diện đều **bỏ qua mọi dòng không phải JSON** thay vì báo lỗi.

### HW-3 — LCD I2C đang được cấp 3.3 V

Connector `P2` theo PDF: **1 = GND, 2 = 3.3 V, 3 = SDA, 4 = SCL**. Module HD44780 kèm
PCF8574 bán phổ thông thường cần 5 V; ở
3.3 V màn hay bị mờ hoặc trắng hẳn dù bus I2C vẫn giao tiếp bình thường.

Với module LCD yêu cầu 5 V, cấp **VCC 5 V** cho LCD và dùng **mạch chuyển mức I2C hai chiều
3.3 V ↔ 5 V** trên SDA/SCL. Phía ESP32/MAX30102/EEPROM kéo lên 3.3 V, phía LCD kéo lên 5 V;
các khối dùng chung GND. Không nối nguồn 5 V vào chân 3V3 của ESP32.

Không nối thẳng SDA/SCL của backpack cấp 5 V vào ESP32: nhiều backpack có sẵn điện trở kéo
lên VCC. Cũng không bảo đảm hoạt động nếu chỉ bỏ pull-up 5 V rồi kéo lên 3.3 V: PCF8574 quy
định mức HIGH tối thiểu là `0.7 × VCC`, tức **3.5 V khi VCC = 5 V**. Xem bảng đặc tính điện
trong [datasheet PCF8574 của TI](https://www.ti.com/lit/ds/symlink/pcf8574.pdf).

Bus dùng chung phải chạy **100 kHz**, là giới hạn của PCF8574. Cả `Wire.begin()` và
`MAX30105::begin()` phải dùng `cfg::i2c::kClockHz`; thư viện MAX3010x tự đặt lại tốc độ bus
trong `begin()`, nên không được truyền `I2C_SPEED_FAST` (400 kHz).

### HW-4 — Không có RTC nuôi pin

ESP32 mất thời gian thực mỗi lần cắt nguồn. Hệ thống xử lý bằng cách đồng bộ giờ từ máy tính
lúc bắt tay — xem mục *Nguồn thời gian* trong [giao-thuc.md](giao-thuc.md).

Cách này đủ cho yêu cầu hiện tại vì `millis()` vẫn chạy suốt lúc mất kết nối. Nếu về sau cần
dấu thời gian tuyệt đối sống sót qua **mất điện**, phải bổ sung DS3231 — nằm ngoài phạm vi
đợt nâng cấp này.

### HW-5 — USB cũng cấp nguồn cho ESP32

Cáp USB nối MacBook/PC với DevKit vừa truyền UART vừa cấp nguồn. PDF cho thấy OUT+ của
LM2596 vào net `3.3V`, nối trực tiếp với 3V3 của U1; không thể hiện diode/jumper chọn nguồn
trên đường này. Nguồn ngoài có thể nối chung với đầu ra ổn áp trên DevKit đang được USB cấp
điện. Chưa xác minh mạch nguồn bên trong DevKit thực tế; không mặc định rằng có thể cắm
đồng thời cả hai nguồn. Cần kiểm tra riêng, chưa kết luận nguồn là nguyên nhân gây lỗi flash.

Khi chẩn đoán/nạp, dùng ESP32 tháo rời và **chỉ nối USB**, như trường hợp T-BOOT đã kiểm
chứng trong [bring-up-checklist.md](bring-up-checklist.md). Khi lắp trở lại, kiểm tra đúng
chân nguồn, hướng cắm và jumper/cách ngắt đường nguồn ngoài cho ESP32 trước khi nối USB.
Chỉ rút điện adapter chưa bảo đảm đường ra LM2596 đã được tách khỏi nguồn USB trên DevKit.
Không tự nối 5 V vào 3V3.

Để vừa cấp adapter vừa giao tiếp máy tính lâu dài, cần kiểm tra/thiết kế cách chọn hoặc
cách ly nguồn phù hợp với board thực tế. Hướng dẫn [nguồn cho DevKitC của Espressif](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html#power-supply-options)
cũng yêu cầu chọn một trong các đường USB, 5V hoặc 3V3; đây là tài liệu tham khảo cho
DevKitC, không thay thế sơ đồ nguồn của DevKit V1 đang sử dụng.

### HW-6 — PDF nối nhầm I2C vào chân flash SD1/SD0

Trong khối ESP32 của PDF, U1 có các kết nối sau:

| Tín hiệu | Đang nối trên PDF | Điểm nối đúng cho firmware |
|---|---|---|
| SDA, net tên `IO21` | U1 **22**, tên chân **SD1** | U1 **33**, tên chân **IO21** |
| SCL, net tên `IO22` | U1 **21**, tên chân **SD0** | U1 **36**, tên chân **IO22** |

Chân IO21 và IO22 thật của U1 trong PDF đang đánh dấu **không nối**. Tên net `IO21`/`IO22`
không biến SD1/SD0 thành GPIO21/GPIO22. Các số U1 ở bảng trên là số chân **trên symbol của
PDF**, không phải số GPIO hay hướng dẫn đếm chân trên board thật; phải đối chiếu đúng
DevKit/footprint và đo thông mạch.

Theo [datasheet ESP32-WROOM-32, mục Pin Description](https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html),
SD0/SD1 tương ứng GPIO7/GPIO8 và được nối với SPI flash trong module. Nếu PCB thực tế theo
kết nối này, tải từ LCD/MAX30102/EEPROM nằm trên đường flash, có thể gây đúng hiện tượng
`invalid header: 0xffffffff` và không đọc được mã flash khi ESP32 cắm vào mạch nền. Đây là
giả thuyết được hỗ trợ bởi sơ đồ và phép thử tháo rời; cần đo PCB để xác nhận đường nối thật.
**Không sửa firmware sang GPIO7/GPIO8 để chạy theo kết nối lỗi.**

Kiểm tra khi đã **rút USB và adapter**, tháo ESP32 khỏi socket:

1. Xác minh số chân/thứ tự chân của board thật khớp socket và footprint. Nếu là board
   30 chân trong khi sơ đồ dùng 38 chân, phải đối chiếu lại từng tín hiệu, không cắm theo
   vị trí ước lượng. Dò từ P2 chân 3 (SDA) tới socket: phải tới IO21 thật, không tới SD1.
2. Dò từ P2 chân 4 (SCL) tới socket: phải tới IO22 thật, không tới SD0.
3. Dò thêm SDA/SCL của P5 EEPROM và P6 MAX30102 vì cả ba dùng chung bus; kiểm tra không
   nối tắt SDA với SCL hay GND. Không suy chân GPIO từ số thứ tự chân connector.
4. Nếu xác nhận đi nhầm: cần sửa sơ đồ và đường mạch/đấu nối để tách bus khỏi SD0/SD1 rồi
   đưa về GPIO21/GPIO22. Chỉ kéo thêm dây sang chân đúng mà vẫn giữ đường cũ chưa giải
   phóng được flash. Hiện chưa có file PCB để chỉ chính xác đường nào cần sửa trên board.
5. Sau khi kiểm tra/sửa xong đường I2C và kiểm tra nguồn theo HW-5, mới cấp nguồn thử lại:
   xác minh ESP32 boot được, rồi kiểm tra `selftest` và LCD theo T-I2C.
