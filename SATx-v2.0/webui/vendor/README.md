# vendor/

Thư viện ngoài được **tải sẵn về đây** thay vì nạp từ CDN.

Lý do: trạm đo thường đặt trong phòng lab hoặc phòng khám không có Internet.
Bản giao diện mẫu trước đây nạp jsQR và jsPDF từ cdnjs — mất mạng là mất luôn
chức năng quét QR và xuất PDF, mà không báo lỗi gì rõ ràng.

| Tệp                | Thư viện | Phiên bản | Giấy phép  |
|--------------------|----------|-----------|------------|
| `jsQR.min.js`      | jsQR     | 1.4.0     | Apache-2.0 |
| `jspdf.umd.min.js` | jsPDF    | 2.5.1     | MIT        |

Cập nhật khi cần:

```sh
curl -fsSL https://cdn.jsdelivr.net/npm/jsqr@1.4.0/dist/jsQR.js        -o jsQR.min.js
curl -fsSL https://cdn.jsdelivr.net/npm/jspdf@2.5.1/dist/jspdf.umd.min.js -o jspdf.umd.min.js
```
