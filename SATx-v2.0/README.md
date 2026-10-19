# SATx — Trạm đo chỉ số sức khoẻ

Đo bốn chỉ số: **cân nặng, chiều cao, nhịp tim, SpO₂**, tính BMI, hiển thị đồng bộ trên màn
hình LCD của máy và giao diện web trên máy tính.

Đây là bản nâng cấp của dự án mẫu trong [`Mẫu dự án/`](Mẫu%20dự%20án/):

| | Dự án mẫu | Bản này |
|---|---|---|
| Vi điều khiển | PIC16F877A (CCS C) | **ESP32-WROOM-32** (Arduino) |
| Giao diện | WinForm C# | **Web cục bộ trong Chrome** |
| Kênh truyền | UART 9600, text thô | **UART 115200 + WiFi dự phòng**, JSON-lines |
| Lưu trữ | không có | **EEPROM AT24C256** làm bộ đệm khi mất kết nối |
| Nhập liệu | bàn phím ma trận 4×4 | hai nút Start / Reset |
| Cấu trúc mã | một hàm `main()` 485 dòng | một file Arduino, chia thành các hàm theo công việc |

## Chạy thử ngay, chưa cần mạch

```sh
cd webui && ./serve.sh
```

Mở `http://localhost:8080` → **Cài đặt** → *Nguồn dữ liệu* → **Mô phỏng**.

Chế độ mô phỏng nói đúng giao thức như firmware thật, kể cả kịch bản mất kết nối → lưu bộ đệm
→ nối lại → đẩy lên. Đủ để chạy trọn vòng: đo, quét QR CCCD, xuất PDF, lịch sử, xuất CSV.

> Phải mở qua `localhost`, không mở thẳng tệp `.html`. Chrome chặn camera và cổng USB trên
> `file://`. `serve.sh` chỉ là server tĩnh, không có backend.

## Nạp firmware

```sh
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 firmware/SATx
arduino-cli upload  --fqbn esp32:esp32:esp32doit-devkit-v1 -p /dev/cu.usbserial-XXXX firmware/SATx
```

Thư viện cần cài (Arduino IDE → Library Manager, hoặc `arduino-cli lib install`):

- `SparkFun MAX3010x Pulse and Proximity Sensor Library`
- `HX711`
- `hd44780`
- `ArduinoJson` (phiên bản 7)
- `WebSockets` (Markus Sattler)

## Chạy test

Không cần board, không cần cảm biến:

```sh
cd firmware/tests && ./run.sh
```

Chạy trực tiếp `SATx.ino` với phần cứng giả lập: **234 kiểm tra**, gồm 133 cho EEPROM + CRC,
39 cho giao thức JSON-lines và 62 cho chu trình đo/kết nối/NVS ở hai chế độ cảm biến.
Bộ test này kiểm tra logic; cần thử trên mạch thật để xác nhận số đo và kết nối vật lý.

## Cấu trúc

```
firmware/
├── SATx/SATx.ino            toàn bộ ứng dụng Arduino trong một file
├── LcdLedTest/              sketch thử riêng LCD và hai LED
├── LedBlinkTest/            sketch thử LED bằng pull-up nội
└── tests/                   test chạy trên máy tính

webui/
├── index.html  css/  serve.sh
├── js/         protocol · state · transport/{uart,wifi,mock,link} · ui/*
└── vendor/     jsQR, jsPDF, font — tải sẵn để chạy được khi không có mạng

docs/
├── cau-hinh-chan-io.md      bảng chân + cảnh báo phần cứng
├── giao-thuc.md             đặc tả JSON-lines, chuyển kênh, bố cục EEPROM
├── luu-do-thuat-toan.md     3 lưu đồ (dự án mẫu cần 7)
├── hieu-chuan.md            quy trình hiệu chuẩn cân và chiều cao
├── so-do-khoi.md            sơ đồ khối cập nhật
└── bring-up-checklist.md    trình tự kiểm tra khi lắp ráp
```

## Đọc mã Arduino

Mở [`firmware/SATx/SATx.ino`](firmware/SATx/SATx.ino) trong Arduino IDE. Đọc `setup()`
trước để thấy thứ tự khởi tạo, rồi đọc `loop()` để theo dõi hoạt động lặp lại của máy.
Các phần cảm biến, LCD/LED/nút nhấn, hiệu chuẩn, EEPROM và giao tiếp được chia thành các
hàm thông thường trong cùng file; không cần đi qua nhiều lớp hay thư mục.

Chân IO, ngưỡng và mốc thời gian nằm trong `namespace cfg` ở đầu file. Khi cần hiểu sâu
một bước, tìm phần hàm tương ứng sau khi đã đọc `loop()`. Cách tổ chức này giữ nguyên
chu trình đo, giao thức UART/WiFi, hiệu chuẩn NVS và định dạng dữ liệu EEPROM. Hai sketch
thử phần cứng chạy độc lập với các hằng số cục bộ khớp cấu hình SATx.

## Trước khi tin số đo

Đọc **[docs/cau-hinh-chan-io.md](docs/cau-hinh-chan-io.md)** — có các cảnh báo phần cứng phát
hiện khi đọc schematic và kiểm tra phần cứng. Đáng chú ý nhất:

- **HW-6 (ưu tiên kiểm tra trước khi lắp ESP32):** PDF IO nối net I2C `IO21`/`IO22` vào
  **SD1/SD0** của U1, là chân flash. Cần đo PCB, tách đường nối nhầm nếu có và đấu về
  GPIO21/GPIO22 thật. Lỗi này có thể làm ESP32 không boot khi cắm vào mạch nền.
- **HW-1**: opto PC817 trên đường ECHO có thể đảo mức. Nếu đảo mà chưa sửa hằng số, phép đo
  chiều cao cho ra con số trông hợp lý nhưng sai hoàn toàn.
- **HW-2**: DevKit đã có sẵn CH340 trên GPIO1/3, mà schematic thêm connector P3 cũng vào đúng
  hai chân đó. Cắm cả hai cùng lúc là tranh chấp bus.

Sau đó theo **[docs/bring-up-checklist.md](docs/bring-up-checklist.md)** và
**[docs/hieu-chuan.md](docs/hieu-chuan.md)**.

## Thiết kế giao diện

Bản thiết kế đã duyệt (hướng *Y tế thân thiện*) và hai hướng đã cân nhắc:
<https://claude.ai/code/artifact/dff793cf-bb75-4a52-a641-e2075c0d148d>
