# Sơ đồ khối hệ thống

Bản cập nhật của `Mục yêu cầu SATx/Bản sao của Sơ đồ khối`, phản ánh những thay đổi của đợt
nâng cấp: đổi MCU sang ESP32, bỏ bàn phím ma trận, thêm EEPROM, và chuyển giao diện lên web.

```mermaid
flowchart LR
    subgraph NGUON["Khối nguồn"]
        AD[Adapter 220 V → 5 V DC]
        LM[LM2596<br/>5 V → 3.3 V]
        AD --> LM
    end

    subgraph THU["Khối thu dữ liệu"]
        HC[HC-SR04<br/>chiều cao]
        LC[Loadcell]
        MAX[MAX30102<br/>nhịp tim · SpO₂]
        BTN[Nút Start / Reset]
    end

    subgraph TRUNGGIAN["Khối trung gian"]
        HX[HX711<br/>ADC 24 bit]
        OPTO[Opto PC817<br/>cách ly ECHO]
    end

    subgraph MCU_["MCU"]
        ESP[ESP32-WROOM-32<br/>DevKit V1]
    end

    subgraph LUU["Khối xuất và lưu trữ"]
        LCD[LCD I2C 16×2]
        LED[Đèn đỏ / xanh]
        EE[EEPROM AT24C256<br/>bộ đệm khi mất kết nối]
    end

    subgraph GIAOTIEP["Khối giao tiếp"]
        CH[USB-UART CH340]
        WIFI[WiFi 2.4 GHz]
    end

    subgraph PC_["Máy tính"]
        WEB[Giao diện web cục bộ<br/>Chrome · localhost]
    end

    LM -->|3.3 V| ESP
    LM -->|3.3 V| THU
    AD -->|5 V| HC

    LC -->|4 dây| HX
    HX -->|2 dây| ESP
    HC -->|TRIG| ESP
    HC -->|ECHO| OPTO --> ESP
    MAX -->|I2C| ESP
    BTN -->|2 chân| ESP

    ESP -->|I2C| LCD
    ESP -->|2 chân| LED
    ESP <-->|I2C| EE

    ESP <-->|UART 115200| CH
    ESP <-.->|dự phòng| WIFI

    CH <-->|kênh chính| WEB
    WIFI <-.->|WebSocket<br/>satx.local:81| WEB
```

## Khác với sơ đồ gốc

| Khối | Dự án mẫu | Bản này | Lý do |
|---|---|---|---|
| MCU | PIC16F877A | ESP32-WROOM-32 | Có sẵn WiFi, đủ RAM cho thuật toán SpO₂ tham chiếu |
| Nhập liệu | Bàn phím ma trận 4×4 | Hai nút Start / Reset | Mọi thao tác khác đã chuyển lên giao diện web |
| Lưu trữ | không có | EEPROM AT24C256 | Giữ kết quả khi mất kết nối hoàn toàn |
| Giao diện | WinForm C# | Web cục bộ trong Chrome | Không cần cài đặt, chạy được trên mọi máy |
| Kênh truyền | chỉ UART | UART chính + WiFi dự phòng | Yêu cầu dự án |

Ba đường nét đứt trong sơ đồ là **đường dự phòng**: WiFi chỉ nhận việc khi UART mất tín hiệu
quá 3 giây. Xem [giao-thuc.md](giao-thuc.md), mục *Chuyển kênh UART ↔ WiFi*.
