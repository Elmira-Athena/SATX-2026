# Hiệu chuẩn

Hai đại lượng bắt buộc phải hiệu chuẩn trước khi số đo có ý nghĩa. Cả hai đều lưu trong NVS
(flash trong của ESP32) nên sống sót qua mất điện, và đều chỉnh được từ mục **Cài đặt** của
giao diện web.

Yêu cầu dự án viết: *"cái này thì code đại trước đợi phần cứng có thì mới chuẩn được"*. Mục
này chính là chỗ để làm việc đó khi có phần cứng — không phải sửa lại mã nguồn.

## Cân nặng (HX711 + loadcell)

Công thức: `kg = (raw_trung_bình − offset) / scale`

### Bước 1 — Trừ bì

1. Dọn sạch bàn cân, không để vật gì lên.
2. Cài đặt → Hiệu chuẩn → **Trừ bì**.

Firmware lấy 7 mẫu, bỏ mẫu lớn nhất và nhỏ nhất, lấy trung bình 5 mẫu còn lại làm `offset`.

Lưu ý: firmware **cũng tự trừ bì lúc khởi động** để bù trôi nhiệt của loadcell. Nếu lúc bật máy
có người đang đứng trên bàn cân thì bước đó thất bại và hệ thống giữ lại giá trị đã lưu trong
NVS — an toàn, nhưng nên bật máy khi bàn cân trống.

### Bước 2 — Đặt hệ số bằng quả cân chuẩn

1. Đặt một quả cân đã biết chính xác khối lượng lên bàn cân. Nên dùng quả **5 kg trở lên** —
   quả càng nhẹ thì sai số tương đối của hệ số càng lớn.
2. Nhập đúng khối lượng vào ô *Quả cân chuẩn (kg)*.
3. Bấm **Đặt hệ số**.

Firmware tính `scale = (raw_hiện_tại − offset) / khối_lượng_đã_biết`.

Nếu chênh lệch raw quá nhỏ (dưới 1 đếm), lệnh trả về lỗi `NOISY` — gần như chắc chắn là quên
đặt quả cân lên.

### Kiểm tra lại

Đặt một vật khác đã biết khối lượng và đo. Sai lệch nên dưới 0.5 %. Nếu sai lệch tăng dần theo
khối lượng thì hệ số sai; nếu sai lệch là một hằng số thì cần trừ bì lại.

Giá trị mặc định `2852.35` lấy từ dự án mẫu, **chỉ là chỗ giữ chân** — mỗi loadcell một khác.

## Chiều cao (HC-SR04)

Công thức: `chiều cao = độ_cao_lắp_cảm_biến − khoảng_cách_đo_được`

1. Đo bằng thước thật khoảng cách từ **mặt sàn** (hoặc mặt bàn cân, tuỳ chỗ người đứng) tới
   **mặt cảm biến**.
2. Nhập vào ô *Cao lắp (cm)* trong Cài đặt → Hiệu chuẩn.
3. Bấm **Đặt cao lắp**.

Giá trị mặc định `200.0 cm` là ước lượng. Dự án mẫu dùng hằng số `20` trong biểu thức
`20 - khoảng_cách` — đó rõ ràng là giá trị tạm lúc thử trên bàn, không phải chiều cao lắp thật.

### Trước khi tin số đo — kiểm tra cực tính ECHO

Xem cảnh báo **HW-1** trong [cau-hinh-chan-io.md](cau-hinh-chan-io.md). Đường ECHO đi qua opto
PC817 nên có thể bị đảo mức. Nếu đảo mà chưa sửa `kEchoActiveLevel`, `pulseIn()` sẽ đo đúng
phần **nghỉ** giữa hai xung thay vì bề rộng xung — cho ra một con số trông hợp lý nhưng sai
hoàn toàn, và hiệu chuẩn kiểu gì cũng không cứu được.

Cách kiểm tra nhanh: đặt một vật ở khoảng cách đã biết (ví dụ 50 cm) và so số đo thô. Nếu lệch
xa và không tuyến tính theo khoảng cách thật, hãy đổi `kEchoActiveLevel` sang giá trị còn lại
rồi nạp lại.

### Ngưỡng hợp lệ

Firmware loại bỏ mẫu ngoài khoảng 2–400 cm (giới hạn vật lý của HC-SR04), rồi loại kết quả
ngoài khoảng 50–250 cm (chiều cao người). Đo 5 lần và lấy **trung vị**, không lấy trung bình —
trung vị loại được mẫu ngoại lai khi người đo cựa quậy, còn trung bình thì không.

## Nhịp tim và SpO₂ (MAX30102)

**Không cần hiệu chuẩn.** Thuật toán tham chiếu của Maxim trong thư viện SparkFun MAX3010x tự
xử lý và trả kèm cờ hợp lệ.

Nếu kết quả hay bị bỏ trống, kiểm tra theo thứ tự:

1. Ngón tay phải **phủ kín** cửa sổ cảm biến, đặt nhẹ — ấn mạnh sẽ chặn mạch máu.
2. Ngưỡng phát hiện ngón tay (`kFingerOnThreshold` = 45000, `kFingerOffThreshold` = 35000) có
   thể cần chỉnh theo module thực tế. Xem giá trị IR thô bằng bài test `T-MAX30102`.
3. Ánh sáng mạnh chiếu thẳng vào cảm biến gây nhiễu — che bớt.

Tất cả tham số nằm trong `namespace cfg::pulseox` của
[`SATx.ino`](../firmware/SATx/SATx.ino).
