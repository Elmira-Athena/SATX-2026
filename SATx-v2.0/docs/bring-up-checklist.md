# Danh sách kiểm tra khi lắp ráp

Chạy theo đúng thứ tự. Mỗi bài test độc lập với bài sau, nên gặp lỗi ở đâu thì dừng và xử lý
ở đó — đừng bỏ qua để chạy tiếp.

Trước khi bắt đầu, đọc mục **Cảnh báo phần cứng** trong
[cau-hinh-chan-io.md](cau-hinh-chan-io.md).

**Trước khi cắm ESP32 vào mạch nền:** đọc **HW-6**. PDF IO mới đối chiếu cho thấy bus I2C
đang nối vào SD1/SD0 của U1, là chân flash. Phải đo thông mạch khi tắt nguồn để xác nhận
PCB thực tế, không chỉ kiểm tra tên net `IO21`/`IO22`.

## T-0 — Nguồn

| Kiểm tra | Kỳ vọng |
|---|---|
| Điện áp ra LM2596 | 3.30 V ± 0.05 V, đo **trước khi** cắm ESP32 |
| Dòng lúc chạy | dưới 500 mA (đỉnh WiFi có thể chạm ~350 mA) |
| Chân 3V3 trên DevKit | không sụt dưới 3.15 V khi WiFi phát |

Cấp sai điện áp cho ESP32 là hỏng board vĩnh viễn. Đừng bỏ qua bước này.

USB từ máy tính cũng cấp nguồn cho DevKit. Khi chưa xác minh mạch chọn/cách ly nguồn,
kiểm tra ESP32 tháo rời với **chỉ USB**; xem **HW-5** trong
[cau-hinh-chan-io.md](cau-hinh-chan-io.md) trước khi vừa cắm USB vừa cấp adapter.

## T-1 — Biên dịch và nạp

```sh
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 firmware/SATx
arduino-cli upload  --fqbn esp32:esp32:esp32doit-devkit-v1 -p /dev/cu.usbserial-XXXX firmware/SATx
```

Kỳ vọng: khoảng 76 % bộ nhớ chương trình, 16 % RAM.

### T-LCDLED — Sketch thử riêng LCD và hai LED

Theo yêu cầu thử độc lập, dùng
[`firmware/LcdLedTest/LcdLedTest.ino`](../firmware/LcdLedTest/LcdLedTest.ino).
Sketch khai báo cục bộ các hằng số khớp phần cấu hình của `SATx/SATx.ino`:
SDA GPIO21, SCL GPIO22, LED đỏ GPIO33, LED xanh GPIO32, bus 100 kHz. Chỉ cần ESP32 core
và thư viện `hd44780`; không khởi tạo cảm biến, WiFi hoặc ghi EEPROM/NVS.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 firmware/LcdLedTest
arduino-cli upload --fqbn esp32:esp32:esp32doit-devkit-v1 -p /dev/cu.usbserial-XXXX --upload-property upload.speed=115200 firmware/LcdLedTest
```

Nạp khi ESP32 tháo rời, chỉ nối USB. Sau khi nạp, rút USB và adapter trước khi lắp lại.
Chỉ cấp nguồn theo đường đã kiểm tra ở T-0/HW-5. Sketch test chạy độc lập, không cần thư mục
SATx đi kèm. Khi đổi chân phần cứng, đối chiếu các hằng số ở đầu sketch test với phần cấu hình
của `SATx.ino`.

Kết quả kỳ vọng sau bước khởi tạo/kiểm tra LCD:

1. Đỏ sáng riêng 2 giây.
2. Xanh sáng riêng 2 giây.
3. Cả hai GPIO được giữ HIGH để bật hai LED.
4. LCD dòng 1 hiện `SATx LCD TEST`, dòng 2 đổi theo giai đoạn và cuối cùng hiện
   `LEDS: BOTH ON`.

Nếu LCD khởi tạo thất bại, chuỗi thử LED vẫn chạy. Sơ đồ dùng chung R3 cho hai LED,
nên độ sáng khi bật cùng lúc có thể khác lúc thử riêng; cần quan sát cả hai giai đoạn
thử riêng trước khi kết luận một LED hỏng.

Serial 115200 in địa chỉ I2C tìm được, `LCD_INIT=0 (OK)` khi khởi tạo LCD thành công và
`ALIVE up=... lcd=... phase=...` mỗi 2 giây. `phase=2` nghĩa là đang giữ cả hai GPIO bật.
ESP32 tháo rời không có LCD sẽ báo LCD thất bại, đây là kết quả bình thường của phép thử đó.
Nếu chỉ còn log ROM `invalid header` và không có `BOOT: SATx LCD + LED TEST`/`ALIVE`,
sketch chưa chạy; kiểm tra T-BOOT/HW-6.

Sketch này thay thế ứng dụng đang chạy trên ESP32 trong lúc thử. Nạp lại `firmware/SATx`
khi muốn dùng chức năng đo bình thường.

**Kết quả phép thử đã thực hiện:**

- Nạp `LcdLedTest` thành công, công cụ xác minh hash các vùng đã ghi. Khi ESP32 cắm riêng
  USB, log `ALIVE` có thời gian chạy tăng từ **40 đến 54 giây**, `phase=2`. `lcd=FAILED`
  là đúng khi LCD đang tháo rời; phép thử này xác nhận sketch chạy, chưa xác nhận LED/LCD
  vật lý trên mạch nền.
- Sau khi người dùng gắn ESP32 vào mạch nền và cấp riêng adapter: **LCD vẫn ô vuông,
  cả hai LED đều tắt**, kể cả giai đoạn thử riêng.
- Trong `setup()`, GPIO LED đỏ đã được đặt HIGH **trước** `Serial.begin()`, `Wire.begin()`
  và khởi tạo LCD. LCD khởi tạo thất bại thông thường không ngăn lệnh bật LED đỏ này.
  Cần phân biệt ESP32 không chạy tới `setup()`/nguồn lỗi với đường LED, cực LED hoặc
  vị trí chân sai; chưa thể chỉ từ quan sát đèn mà xác nhận chính xác một lỗi PCB.
- Thử tiếp khi ESP32 vẫn gắn PCB nhưng **jack adapter đã rút khỏi PCB**, chỉ cấp USB:
  log boot báo `flash_parts: partition 3 invalid magic number 0x0`,
  `Failed to verify partition table`, `load partition table error!`; chưa vào sketch test.
  Đọc vùng `0x8000` bằng flasher và so với file đã nạp: **3072/3072 byte khớp**, cùng
  SHA-256. Bảng phân vùng lưu trong flash đọc được qua công cụ nạp, nhưng đọc lúc boot
  đang thất bại. Lỗi không chỉ xuất hiện khi dùng adapter; vẫn cần kiểm tra đường nguồn
  trên PCB và tải/đấu nối đường flash. Cấu hình lần thử này là **DIO, flash 80 MHz**.
- Thử giữ nguyên sketch/chân IO, đổi riêng cấu hình sang **DIO, flash 40 MHz** và nạp
  đầy đủ khi PCB chỉ cấp USB: công cụ nạp xác minh hash thành công, nhưng lúc boot vẫn
  lỗi `Checksum failed. Calculated 0x57 read 0x5a`, `No bootable app partitions`, có lần
  còn `csum err:0xe3!=0x47`. Log `clock div:2` xác nhận đang thử cấu hình 40 MHz.
  Kiểm tra file cục bộ bằng `esptool image-info`: checksum và validation hash của cả
  ứng dụng/bootloader đều hợp lệ (checksum ứng dụng `0x5a`, bootloader `0x47`).
  Chưa vào được sketch test trên PCB; giảm tốc độ flash không giải quyết được lỗi này.
- **Đối chiếu cùng firmware 40 MHz, không nạp lại giữa hai lần:** sau khi rút USB,
  tháo ESP32 khỏi PCB rồi chỉ cắm USB lại, log có `BOOT: SATx LCD + LED TEST`,
  `LED: RED`, `LED: GREEN`, `LEDS: BOTH ON`, và `ALIVE` tăng qua **2, 4, 6, 8 giây**
  với `phase=0,1,2`. Không có lỗi checksum như lúc gắn PCB. Không có thiết bị I2C trên
  ESP32 tháo rời nên `I2C_SCAN_END count=0` / `LCD_INIT=-4` là bình thường.
  Phép đối chiếu xác nhận trạng thái gắn mạch nền ảnh hưởng khả năng boot; **chưa xác nhận
  bằng phép đo** rằng lỗi nằm chính xác ở SDA/SCL, nguồn hay vị trí nối khác. Bước tiếp
  theo là đo nguồn 3V3 trên ESP32 và thông mạch HW-6; không coi nạp thêm sketch là cách
  sửa đã được chứng minh. Bản dùng trong phép đối chiếu này là sketch LCD/LED ở 40 MHz.

Để tái lập riêng phép thử 40 MHz, dùng
`--fqbn esp32:esp32:esp32doit-devkit-v1:FlashFreq=40` cho cả compile và upload. Đây là thay đổi
tốc độ **SPI flash**, độc lập với tốc độ **I2C 100 kHz** trong phần cấu hình của `SATx.ino`.

Bước kiểm tra tiếp là đọc log boot khi gắn mạch với nguồn được tách đúng, hoặc đo điện áp
3V3 ngay trên ESP32 và thông mạch hai đường I2C/flash theo HW-6. Nếu có USB-UART rời,
có thể dùng đường UART P3 TX/RX/GND để đọc log khi mạch chạy adapter, không nối chân VCC
của USB-UART và không cắm thêm USB trực tiếp vào ESP32. Phải xác minh mức logic UART
phù hợp 3.3 V trước khi nối.

### T-LED-ROI — LED tháo rời, người dùng không có điện trở rời

Điện trở hạn dòng R3 nằm trên PCB. Khi tháo cả hai dây LED khỏi PCB, LED không còn nối
tiếp R3; không dùng sketch xuất GPIO HIGH trực tiếp cho cách đấu LED rời không có điện trở.

Theo yêu cầu thử LED rời, dùng
[`firmware/LedBlinkTest/LedBlinkTest.ino`](../firmware/LedBlinkTest/LedBlinkTest.ino).
Phiên bản này dành riêng cho trường hợp **chưa có điện trở rời**: GPIO33 luôn là đầu vào,
đổi giữa `INPUT_PULLUP` và `INPUT_PULLDOWN` mỗi 1 giây. Điện trở kéo nội khoảng 45 kΩ theo
[tài liệu GPIO của Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html)
giới hạn dòng thử xuống mức rất nhỏ. LED có thể chỉ nháy mờ khi nhìn trong chỗ tối.
**Không thấy sáng chưa đủ để kết luận LED hoặc GPIO hỏng.**

```sh
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1:FlashFreq=40 firmware/LedBlinkTest
arduino-cli upload --fqbn esp32:esp32:esp32doit-devkit-v1:FlashFreq=40 -p /dev/cu.usbserial-XXXX --upload-property upload.speed=115200 firmware/LedBlinkTest
```

1. Giữ ESP32 tháo rời, LED chưa nối, chỉ cắm USB để nạp sketch này trước.
2. Xác minh Serial có `BOOT: SATx WEAK LED BLINK TEST - INPUT PULLUP ONLY`.
   Các dòng `LED_WEAK` phải báo `in=1 out=0`; khi ON là `pu=1 pd=0`, khi OFF là
   `pu=0 pd=1`. Sketch đọc lại cấu hình GPIO để in các trường này.
3. Rút USB, nối **anode (+) LED → GPIO33**, **cathode (−) LED → GND** của ESP32.
   Không nối hai dây này vào 3V3/5V; mạch nền và adapter không tham gia phép thử.
4. Cắm lại USB, quan sát LED ở chỗ tối. Chu kỳ thử: 1 giây kéo lên, 1 giây kéo xuống.
   Đừng suy cực LED chỉ từ màu dây; nếu cần đổi chiều LED thì rút USB trước.

Đây là phép thử dòng nhỏ, không thay thế bài thử LED/GPIO với điện trở nối tiếp phù hợp
(330 Ω–1 kΩ). Khi dùng firmware khác hoặc đổi GPIO sang OUTPUT, phải bổ sung điện trở rời
hoặc nối qua điện trở hạn dòng trên PCB trước.

**Trạng thái nạp mới nhất:** đã nạp `LedBlinkTest` ở DIO/40 MHz và xác minh hash thành công.
Đọc log sau reset khi ESP32 tháo rời: `LED_WEAK ON/OFF` luân phiên từ 0 đến 9 giây.
Cả 10 lần đọc cấu hình GPIO33 đều có `in=1 out=0 status=0`; ON là `pu=1 pd=0`, OFF là
`pu=0 pd=1`. Xác nhận được chế độ GPIO và vòng lặp thử; kết quả quan sát LED rời còn chờ
người dùng nối LED đúng cực và phản hồi.

### T-BOOT — `invalid header: 0xffffffff`

Nếu Serial Monitor lặp lại:

```text
rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
invalid header: 0xffffffff
```

ESP32 chưa đọc được header bootloader hợp lệ ở flash `0x1000`, nên **chưa chạy firmware
SATx**. Lúc này LCD có ô vuông và hai LED tắt không đủ để kết luận chúng bị hỏng.
Nguồn, đường kết nối flash hoặc bản nạp thiếu/sai vị trí đều có thể gây triệu chứng này.
Riêng mạch SATx cần ưu tiên **HW-6: I2C đang nối SD1/SD0 trên PDF** trước khi cắm lại ESP32.
Xem [giải thích của Espressif](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html#invalid-header-value).

1. Đóng Serial Monitor, chọn board **DOIT ESP32 DEVKIT V1**, chọn đúng cổng USB và mở
   `firmware/SATx/SATx.ino`.
2. Dùng **Sketch → Upload** (nút mũi tên) hoặc lệnh `arduino-cli upload` ở T-1 để nạp đầy đủ.
   `Upload Using Programmer` trong ESP32 Arduino core đang dùng chỉ ghi ứng dụng ở
   `0x10000`, nên không khôi phục được bootloader bị thiếu.
3. Kiểm tra log upload đã ghi và xác minh đủ các thành phần:

   | Địa chỉ flash | Thành phần |
   |---|---|
   | `0x1000` | `SATx.ino.bootloader.bin` |
   | `0x8000` | `SATx.ino.partitions.bin` |
   | `0xe000` | `boot_app0.bin` |
   | `0x10000` | `SATx.ino.bin` |

   Đây là offset cho **ESP32 WROOM-32** và cấu hình dự án hiện tại. Dùng các file từ cùng
   một lần build. Nếu tự nạp file `SATx.ino.merged.bin`, file này bắt đầu từ `0x0` và chứa
   cả khoảng trống giữa các phân vùng; nạp file gộp có thể ghi đè vùng NVS chứa cấu hình.
   Ưu tiên cách nạp từng thành phần tự động của Arduino.
4. Mở lại Serial Monitor **115200**, bấm EN/RESET. Firmware chạy tới cuối `begin()` sẽ in
   bản tin `selftest` mô tả LCD/MAX30102/EEPROM.
5. Nếu upload không kết nối, giữ BOOT lúc công cụ báo `Connecting`, thả khi bắt đầu ghi.
   Nếu ghi lỗi giữa chừng, thử **Upload Speed 115200**, cáp USB khác và nguồn ổn định.
6. Nếu upload đã xác minh thành công mà vẫn báo header lỗi, tắt nguồn trước khi tháo ngoại
   vi rồi thử riêng ESP32 với USB. Kiểm tra không mắc ngoại vi vào GPIO6–11 (chân flash),
   không kéo GPIO12 lên HIGH lúc reset (có thể chọn sai điện áp flash), và kiểm tra nguồn
   3.3 V. Xem [hướng dẫn xử lý lỗi của Espressif](https://docs.espressif.com/projects/esptool/en/latest/esp32/troubleshooting.html).

**Nếu công cụ nạp không nhận diện được flash:** đóng Serial Monitor và kiểm tra bằng
`esptool --chip esp32 --port <cong-USB> --baud 115200 --no-stub flash-id` (esptool 5.x).
Nếu thấy `Manufacturer: ff`, `Device: ffff`, `Detected flash size: Unknown` hoặc
`Failed to communicate with the flash chip`, ESP32 vẫn giao tiếp UART nhưng flash chưa
giao tiếp được. Không thể xác nhận đây chỉ là thiếu bootloader, và cần xử lý lỗi giao tiếp
flash trước khi nạp lại.

Tắt nguồn, tháo ngoại vi/thử board ngoài breadboard rồi chỉ cắm USB; thử cáp và nguồn tốt,
sau đó chạy lại `flash-id`. Nếu `Flash voltage set by a strapping pin: 3.3V` thì cấu hình
điện áp theo chân strap đang là 3.3 V, chưa chứng minh điện áp thực tế ổn định. Nếu flash
vẫn trả `ff/ffff` khi board đã được tách riêng và nguồn đã kiểm tra, cần kiểm tra mối hàn,
nguồn chip flash hoặc thử một module ESP32 khác. Đổi cấu hình LCD không giải quyết được
lỗi ở bước này.

**Trường hợp đã kiểm tra trên máy SATx:** khi ESP32 còn gắn mạch nền, cả cách đọc qua stub
và ROM đều không giao tiếp được với flash (`ff/ffff`). Sau khi tháo ESP32 ra và chỉ nối USB,
flash trả mã `68/4016`, dung lượng **4 MB**; đọc được header bootloader tại `0x1000` bắt đầu
bằng `e9`. Nạp đầy đủ firmware thành công và nhận được `pong` với thời gian chạy tăng liên
tục. Đối chiếu PDF sau đó phát hiện net I2C `IO21`/`IO22` đang nối vào SD1/SD0 của U1
(HW-6); cần đo PCB để xác nhận đây có phải đường nối thật gây lỗi. Kiểm tra đường I2C,
hướng cắm và nguồn trước khi lắp lại. Khi chỉ cắm
ESP32 bằng USB, `selftest` báo `lcd:false`, `ppg:false`, `eeprom:false` là bình thường.
Người dùng xác nhận board thực tế **38 chân (19 mỗi bên)**, khớp số chân trên symbol;
hiện chưa có đồng hồ đo nên **chưa xác minh thông mạch PCB**. Không coi kết quả đọc PDF
là xác nhận đã đo đường mạch thực tế, và không chỉ định vị trí cắt/hàn khi chưa có bằng
chứng PCB/đường nối.

## T-I2C — Quét bus

Mở Serial Monitor ở 115200 rồi bấm **EN/RESET trên ESP32**. Firmware in ra kết quả tự kiểm
tra lúc khởi động, ví dụ khi cả ba khối khởi tạo thành công:

```json
{"t":"selftest","lcd":true,"ppg":true,"eeprom":true}
```

Đây là kết quả khởi tạo ba driver, **không phải danh sách địa chỉ quét bus**. Muốn quét bus
và kiểm tra riêng LCD, dùng ví dụ của thư viện `hd44780`:
**File → Examples → hd44780 → ioClass → hd44780_I2Cexp → I2CexpDiag**. Nạp lại `SATx.ino`
sau khi chạy ví dụ chẩn đoán.

| Địa chỉ | Thiết bị | Nếu thiếu |
|---|---|---|
| `0x27` hoặc `0x3F` | LCD (PCF8574) | Kiểm tra nguồn LCD — xem **HW-3** |
| `0x50` | EEPROM AT24C256 | Kiểm tra A0/A1/A2 có nối GND không |
| `0x57` | MAX30102 | Địa chỉ cố định, thiếu là lỗi hàn hoặc nguồn |

Không thiết bị nào trả lời → kiểm tra nối đúng GPIO21/GPIO22 thật theo **HW-6**, mức điện
áp và pull-up I2C trên các module. R4/R5 của PDF là pull-up nút nhấn, không phải I2C.

Bus dùng chung mặc định **100000 Hz** vì PCF8574 chỉ hỗ trợ tối đa 100 kHz. Không tăng lên
400 kHz, kể cả khi MAX30102 hỗ trợ tốc độ đó. Nếu thiết bị vẫn chập chờn, kiểm tra dây dài,
pull-up và mức điện áp theo **HW-3**.

### LCD chỉ hiện ô vuông đen

1. Kiểm tra đúng sketch `firmware/SATx/SATx.ino`, đúng board ESP32 DevKit V1 và thư viện
   `hd44780`. Lệnh khởi tạo phải là `begin(kCols, kRows)` (**16, 2**), không phải **2, 16**.
   Thư viện tự dò địa chỉ LCD; đổi `kAddrLcd` không thay đổi cách tự dò này.
2. Kiểm tra `SDA → GPIO21`, `SCL → GPIO22`, GND chung và nguồn theo **HW-3**. Vặn nhẹ biến
   trở tương phản trên backpack để chữ nhìn rõ; đèn nền sáng chưa chứng minh khởi tạo thành công.
3. Đọc bản tin `selftest`: `lcd:false` nghĩa là khởi tạo LCD thất bại; dùng `I2CexpDiag` để
   phân biệt lỗi bus với lỗi giao tiếp giữa backpack và LCD. Nếu `lcd:true` nhưng chưa thấy
   chữ, kiểm tra lại tương phản, nguồn và các mối hàn backpack.
4. Sau khởi động và trừ bì cân, màn hình phải hiện `San sang` / `Bam START de do`.

## T-LED — Đèn báo

| Trạng thái | Đèn đỏ (IO33) | Đèn xanh (IO32) |
|---|---|---|
| Sẵn sàng | tắt | tắt |
| Đang đo | **nháy 2 Hz** | tắt |
| Đo xong | tắt | sáng |
| Lỗi | sáng liên tục | tắt |

Đèn nháy khi đang đo là cố ý — để phân biệt với trạng thái lỗi (đỏ sáng liên tục).

**Hai LED tắt ngay sau khi nạp là bình thường**: chúng báo trạng thái đo, không báo nguồn.
Bấm Start (IO26 nối xuống GND) hoặc gửi dòng `{"t":"measure"}` qua Serial Monitor để thử
đèn đỏ. Nếu cần xem phản hồi JSON, gửi `{"t":"ping"}` mỗi giây để giữ kênh UART hoạt động.
Đèn xanh chỉ sáng khi chu trình kết thúc và có ít nhất một chỉ số hợp lệ.
Nếu đã bắt đầu đo mà đèn đỏ vẫn tắt, kiểm tra đúng GPIO33/GPIO32, điện trở 330 Ω và cực LED:
firmware xuất HIGH để bật, phù hợp với LED mắc từ GPIO qua điện trở và LED xuống GND.

## T-BTN — Nút nhấn

1. Bấm **Start** (IO26) ở trạng thái sẵn sàng → phải vào chu trình đo.
2. Bấm **Reset** (IO25) bất cứ lúc nào → phải về sẵn sàng ngay.
3. Bấm Start liên tục thật nhanh → chỉ được nhận **một** lần trong 400 ms.

Nút nối xuống GND, dùng pull-up nội. Nếu bấm không ăn, đo xem chân có xuống 0 V thật không.

## T-HCSR04 — Cảm biến siêu âm (quan trọng nhất)

**Trước hết phải xác minh cực tính ECHO.** Xem **HW-1**.

1. Đặt một tấm phẳng cách cảm biến đúng **50 cm** (đo bằng thước).
2. Đọc khoảng cách thô qua Serial Monitor.
3. Nếu số đo lệch xa 50 cm:
   - Đổi `cfg::height::kEchoActiveLevel` sang giá trị còn lại (0 ↔ 1), nạp lại, đo lại.
   - Chỉ một trong hai giá trị cho ra kết quả **tuyến tính** theo khoảng cách thật.
4. Kiểm tra tuyến tính ở 30 cm, 50 cm, 100 cm, 150 cm trước khi kết luận.

Sau khi cực tính đúng, làm tiếp mục *Chiều cao* trong [hieu-chuan.md](hieu-chuan.md).

## T-HX711 — Loadcell

1. Bàn cân trống → đọc raw, ghi lại. Giá trị phải **ổn định**, dao động dưới vài trăm đếm.
2. Ấn nhẹ tay lên bàn cân → raw phải đổi rõ rệt và theo một chiều nhất quán.
3. Nếu raw nhảy loạn: kiểm tra dây loadcell (đỏ/đen/trắng/xanh), che chắn nhiễu, và bảo đảm
   HX711 dùng chung GND với ESP32.
4. Làm mục *Cân nặng* trong [hieu-chuan.md](hieu-chuan.md).

## T-MAX30102 — Nhịp tim và SpO₂

1. Không đặt ngón tay → giá trị IR thô phải **thấp** (dưới ~35000).
2. Đặt ngón tay phủ kín → IR phải vọt lên trên ~45000.
3. Nếu hai ngưỡng này không tách bạch, chỉnh `kFingerOnThreshold` / `kFingerOffThreshold`
   trong phần cấu hình của `SATx.ino` theo giá trị thực đo được.
4. Giữ ngón tay yên 15 giây → phải ra nhịp tim và SpO₂ hợp lệ.

## T-EEPROM — Bộ đệm

Chạy test trên máy tính trước, không cần board:

```sh
cd firmware/tests && ./run.sh
```

Kỳ vọng: **133 kiểm tra đạt / 0 hỏng** cho EEPROM, **39 / 0** cho giao thức,
**36 / 0** cho chu trình dùng nhánh đọc cảm biến và **26 / 0** cho nhánh mô phỏng.
Tất cả chạy trên máy tính với phần cứng giả lập, chưa thay thế kiểm tra trên mạch thật.

Trên phần cứng thật:

1. Rút cáp USB, bấm Start trên máy để đo 2 lần.
2. Cắm lại cáp, mở giao diện.
3. Hai bản ghi phải tự đẩy lên, hiện trong Lịch sử với nhãn **Từ bộ đệm** (nền vàng).
4. Sau khi đẩy xong, `pending` trong bản tin `pong` phải về **0**.

## T-FAILOVER — Chuyển kênh

Cần cấu hình WiFi trước (Cài đặt → WiFi dự phòng → nhập SSID/mật khẩu → Lưu).

| Tình huống | Kỳ vọng |
|---|---|
| Rút cáp lúc máy rảnh | Giao diện chuyển sang WiFi trong dưới 2 giây, hiện thông báo |
| Rút cáp **giữa lúc đang đo** | Kênh **không** đổi cho tới khi đo xong, rồi mới chuyển |
| Cắm lại cáp lúc rảnh | Quay về UART, WiFi ngủ lại, có thông báo |
| Rút cả cáp lẫn WiFi | Kết quả vào EEPROM, `pending` tăng dần |

Lưu ý: khi cắm lại cáp, phải bấm **Chọn cổng** một lần trong Cài đặt. Trình duyệt không cho
mở lại cổng USB nếu không có cú bấm chuột — ràng buộc bảo mật của Web Serial.

## T-UI — Giao diện

Không cần phần cứng:

```sh
cd webui && ./serve.sh
```

Cài đặt → Nguồn dữ liệu → **Mô phỏng**, rồi chạy trọn một vòng:

- [ ] Bấm Đo → tiến trình chạy qua ba bước, đếm ngược đúng
- [ ] Biểu mẫu bệnh nhân tự khoá trong lúc đo
- [ ] Xong → bốn chỉ số hiện ra, BMI có vạch đúng vị trí trên thang
- [ ] Quét QR CCCD → thông tin tự điền vào biểu mẫu
- [ ] Xuất PDF → phiếu mở ra được, chữ không dấu, đủ chỉ số
- [ ] Lịch sử → lọc được, xoá được từng dòng, xuất CSV mở bằng Excel không lỗi font
- [ ] **Giả lập rớt kết nối** → đo 2 lần → **Nối lại** → hai dòng nền vàng xuất hiện
