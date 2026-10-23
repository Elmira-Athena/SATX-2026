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

Lưu ý: firmware **cũng tự trừ bì lúc khởi động** để bù trôi nhiệt của loadcell. Phải bật máy
khi bàn cân trống: nếu đã đặt điện thoại hoặc vật khác lên, khối lượng đó cũng bị tính vào
bì và số đo có thể về 0. Firmware chỉ giữ offset cũ khi không đọc được HX711, không tự nhận
biết có vật đặt trên cân.

Bản dành cho mô hình chấp nhận số đo từ **0 đến 250 kg**, gồm cả vật nhẹ dưới 2 kg như điện
thoại. Bản đo người trước đây loại mọi số dưới 2 kg nên những vật này hiện `--` dù đã đọc
được HX711. Việc bỏ ngưỡng 2 kg không thay thế hiệu chuẩn: muốn số đúng phải dùng vật có
khối lượng đã biết. Ô *Quả cân chuẩn (kg)* dùng đơn vị kg, ví dụ 200 g nhập **0,2**.

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

Mô hình hiện tại lắp cảm biến cách mặt cân **20 cm** nên firmware mặc định `20.0 cm`.
Mặt cân trống cho chiều cao xấp xỉ 0 cm; vật cao 5 cm sẽ cách cảm biến khoảng 15 cm.
Giá trị 180,2 cm khi mặt cân trống phù hợp với việc dùng nhầm mốc 200 cm: `200 − 19,8 = 180,2`.

**Nạp lại firmware không xoá giá trị hiệu chuẩn cũ trong NVS.** Sau khi nạp bản dành cho
mô hình, đặt **Cao lắp = 20 cm** và kiểm tra dòng *Cao lắp cảm biến* trên web đã báo 20 cm.
Nếu sau này lắp để đo người, nhập lại độ cao lắp thật, không dùng 20 cm.

### Khi số đo vật nhỏ bị lệch hoặc dao động

Giữ nguyên cao lắp và vật đo, dùng mặt trên phẳng, đủ rộng, song song mặt cảm biến.
Trong nhật ký web, mỗi lượt đo thật có dòng `HC-SR04 mount=...; echo_us=[...]; valid=.../5`:

- `echo_us` là 5 độ rộng xung gốc theo thứ tự thu, chưa sắp xếp; `0` nghĩa là timeout.
- Khoảng cách từng mẫu là `echo_us / 58` cm; H bằng cao lắp trừ trung vị các khoảng cách hợp lệ.
- `valid` đếm mẫu trong khoảng 2–400 cm. Có 5/5 mẫu hợp lệ chưa chứng minh tín hiệu ổn định.

Số liệu thực nghiệm: cân trống `0,2 / 0,2 / 0,2 cm`, vật cao 4 cm cho `3,2 / 2,1 / 2,2 cm`.
Các kết quả này chưa đủ để suy ra một hệ số bù tin cậy. Thu nhật ký khi cân trống và khi
đo lại cùng vật có mặt trên phẳng trước khi bù độ lệch hoặc đổi cách đọc xung.

### Trước khi tin số đo — kiểm tra cực tính ECHO

Xem cảnh báo **HW-1** trong [cau-hinh-chan-io.md](cau-hinh-chan-io.md). Đường ECHO đi qua opto
PC817 mắc đảo mức theo sơ đồ, nên firmware mặc định `kEchoActiveLevel = 0` (LOW).
Nếu đã thay phần ECHO bằng mạch không đảo mức thì dùng `1` (HIGH). Chọn sai có thể gây
timeout hoặc đo phần nghỉ thay vì bề rộng xung; hiệu chuẩn độ cao lắp không sửa được lỗi này.

Cách kiểm tra nhanh: đặt một vật ở khoảng cách đã biết (ví dụ 50 cm) và so số đo thô. Nếu lệch
xa và không tuyến tính theo khoảng cách thật, hãy đổi `kEchoActiveLevel` sang giá trị còn lại
rồi nạp lại.

### Ngưỡng hợp lệ

Firmware loại bỏ mẫu ngoài khoảng 2–400 cm (giới hạn vật lý của HC-SR04), rồi loại kết quả
ngoài khoảng 0–250 cm (cho phép đo vật nhỏ trong mô hình). Khoảng cách âm hoặc chiều cao âm
vẫn bị loại; số đo gần 0 có thể dao động do cảm biến và cần chỉnh lại cao lắp theo thực tế.
Đo 5 lần và lấy **trung vị**, không lấy trung bình —
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
