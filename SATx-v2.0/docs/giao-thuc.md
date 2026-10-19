# Giao thức JSON-lines

Một bản tin là **một dòng JSON kết thúc bằng `\n`**. Cùng một giao thức dùng cho cả hai kênh:

- **UART** 115200 8N1 qua CH340 (kênh chính)
- **WebSocket** `ws://satx.local:81/` (kênh dự phòng)

Bên nhận **bỏ qua mọi dòng không parse được**. Đây là hành vi cố ý, không phải xử lý lỗi cẩu
thả: cổng UART0 dùng chung với log bootloader của ESP32, nên lúc vừa cắm cáp sẽ thấy những
dòng như `rst:0x1 (POWERON_RESET),boot:0x13`.

Cài đặt: [`firmware/SATx/SATx.ino`](../firmware/SATx/SATx.ino) và
[`webui/js/protocol.js`](../webui/js/protocol.js).

## Giao diện → thiết bị

| Bản tin | Ý nghĩa |
|---|---|
| `{"t":"ping"}` | Nhịp tim kết nối, gửi mỗi 1 giây |
| `{"t":"sync","epoch":1756713600}` | Đồng bộ đồng hồ (giây kể từ 1970) |
| `{"t":"measure"}` | Bắt đầu một phép đo |
| `{"t":"reset"}` | Đưa máy về trạng thái sẵn sàng |
| `{"t":"flush"}` | Yêu cầu đẩy hết bản ghi trong EEPROM lên |
| `{"t":"cal","target":"weight"}` | Trừ bì cân (không có `known_kg`) |
| `{"t":"cal","target":"weight","known_kg":5.0}` | Đặt hệ số cân theo quả cân chuẩn |
| `{"t":"cal","target":"height","mount_cm":198.5}` | Đặt độ cao lắp cảm biến siêu âm |
| `{"t":"cfg","wifi":{"ssid":"...","pass":"..."}}` | Lưu thông tin WiFi vào NVS |
| `{"t":"erase"}` | Xoá logic bộ đệm (chỉ ghi lại header) |
| `{"t":"erase","physical":true}` | Xoá vật lý toàn bộ vùng bản ghi |

## Thiết bị → giao diện

```json
{"t":"pong","link":"uart","up":12345,"pending":0}
{"t":"info","link":"uart","ip":"192.168.1.63","pending":0,"mount_cm":200.0,"w_scale":2852.35,"fw":"SATx-2.0"}
{"t":"state","s":"measuring","step":"ppg","pct":40,"eta":9}
{"t":"result","ts":1756713612,"tsValid":true,"w":62.40,"h":171.2,"hr":78,"spo2":97,"bmi":21.3,"ok":true,"src":"live"}
{"t":"error","code":"NO_WEIGHT","msg":"Buoc do that bai"}
{"t":"log","lvl":"info","msg":"..."}
```

| Trường | Ý nghĩa |
|---|---|
| `link` | Kênh đang hoạt động: `uart`, `wifi` hoặc `none` |
| `up` | Số giây kể từ lúc khởi động |
| `pending` | Số bản ghi còn trong EEPROM chờ đẩy lên |
| `s` | `boot`, `selftest`, `idle`, `measuring`, `done`, `error` |
| `step` | `ppg`, `paused`, `height`, `weight`, `none` |
| `pct` / `eta` | Tiến độ 0–100 và số giây còn lại, phát 4 lần mỗi giây khi đang đo |
| `src` | `live` = đo trực tiếp, `eeprom` = đẩy lên từ bộ đệm |
| `ok` | `true` khi cả bốn chỉ số đều đọc được |

### Trường thiếu là `null`, không phải `0`

Chỉ số nào không đọc được sẽ gửi `null`. Ví dụ khi cảm biến nhịp tim không bắt được ngón tay:

```json
{"t":"result","ts":0,"tsValid":false,"w":70.15,"h":176.0,"hr":null,"spo2":null,"bmi":22.6,"ok":false,"src":"eeprom"}
```

Đây là sửa lỗi có chủ đích so với dự án mẫu. Bản PIC dùng số `0` cho cả "chưa đo" lẫn "đọc
lỗi", khiến giao diện không phân biệt được và hiển thị `0 bpm` như một số đo thật. Nay giao
diện hiện `— —` cho đúng ô đó.

## Nguồn thời gian

ESP32 không có RTC nuôi pin nên không tự biết giờ. Cách xử lý:

1. Giao diện gửi `{"t":"sync","epoch":...}` ngay khi bắt tay.
2. Firmware ghi lại cặp `(epoch, millis())` và suy ra:
   `now = epochAtSync + (millis() − millisAtSync) / 1000`.
3. `millis()` vẫn chạy suốt lúc mất kết nối — đúng kịch bản mà EEPROM cần đến.
4. Chưa từng đồng bộ → `tsValid: false`. Giao diện khi đó dùng thời điểm **nó nhận được** bản
   ghi và đánh dấu `~` trong bảng lịch sử, thay vì in ra một mốc giờ sai.

Cài đặt: [`firmware/SATx/SATx.ino`](../firmware/SATx/SATx.ino).

## Chuyển kênh UART ↔ WiFi

```
UART_ACTIVE   Giao diện gửi ping mỗi 1 s.
              Không thấy ping trong 3 s ────────────────► UART_LOST

UART_LOST     Đánh thức đường dữ liệu WiFi. Dưới 50 ms, vì WiFi vẫn luôn ở chế độ STA
              đã kết nối sẵn — chỉ tắt modem-sleep chứ không quét lại mạng (1–3 s).
              Chờ WebSocket client.

WIFI_ACTIVE   Vẫn lắng nghe UART. Khi có ping UART trở lại:
                · đang đo → chờ đo xong rồi mới chuyển, không đổi kênh giữa chừng
                · rảnh    → về UART ngay, cho WiFi ngủ lại, báo dòng thông báo lên giao diện

NO_LINK       Mất cả hai → bản ghi được cất vào EEPROM.
              Có kênh trở lại → tự đẩy hết lên (src="eeprom") rồi xoá.
```

Điểm mấu chốt của thiết kế: **WiFi không bao giờ bị tắt hẳn.** Đó là điều kiện để đáp ứng yêu
cầu "tối ưu thời gian ngắn nhất khi WiFi thay thế UART". Máy cắm adapter 5 V nên khoảng 20 mA
của modem-sleep không đáng để đánh đổi lấy 1–3 giây chờ kết nối lại.

Phía giao diện, `link.js` phát hiện mất `pong` rồi tự mở WebSocket tới `satx.local` và hiện
thông báo. Một hạn chế của trình duyệt cần biết: **Web Serial không cho mở lại cổng USB nếu
không có cú bấm chuột.** Vì vậy khi cắm lại cáp, người dùng phải bấm "Chọn cổng" một lần — đây
là ràng buộc bảo mật của trình duyệt, không phải thiếu sót của thiết kế.

## Bố cục bản ghi trong EEPROM

AT24C256 (32 KB, trang 64 byte, địa chỉ `0x50`). Xem
[`firmware/SATx/SATx.ino`](../firmware/SATx/SATx.ino).

```
0x0000  Header 16 B   magic "SATX", version, count, head, crc16
0x0010  Vùng bản ghi  16 B mỗi bản, dùng như một vòng tròn — tối đa 2047 bản
```

| Offset | Kiểu | Trường |
|---|---|---|
| 0 | u32 | `epoch` (giây) |
| 4 | u16 | cân nặng, kg × 100 |
| 6 | u16 | chiều cao, cm × 10 |
| 8 | u8 | nhịp tim |
| 9 | u8 | SpO₂ |
| 10 | u16 | BMI × 100 |
| 12 | u8 | cờ hợp lệ từng trường |
| 13 | u8 | dự phòng |
| 14 | u16 | `crc16` (CCITT-FALSE, phủ 14 byte đầu) |

Bản ghi 16 byte được căn thẳng nên **không bao giờ tràn qua ranh giới trang 64 byte** — mỗi
lần lưu chỉ tốn đúng một chu kỳ ghi 5 ms.

Sau khi đẩy hết lên, firmware **xoá logic**: ghi lại header với `count = 0`. Cách này nhanh và
ít hao mòn hơn nhiều so với ghi đè 32 KB. Mục Cài đặt có nút xoá vật lý cho ai cần xoá triệt để.

Đổi sang AT24C32 chỉ cần sửa hai hằng số `kCapacityBytes` và `kPageSizeBytes` trong phần
cấu hình của `SATx.ino`.
