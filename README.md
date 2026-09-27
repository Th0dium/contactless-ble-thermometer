# Firmware — Giám sát thân nhiệt không tiếp xúc qua BLE

Bản khởi đầu (starter build) cho đồ án trong `Requirements_RutGon (1).docx`.
Đã build thành công, **chưa test trên phần cứng thật**.

```
RAM:   41.5% (850 / 2048 bytes)
Flash: 51.8% (16710 / 32256 bytes)   — spec giới hạn ≤75%
```

## Cần gì để chạy

- Board ATmega328P (Arduino Uno hoặc tương đương)
- Cảm biến MLX90614 (module GY-906) — I²C, chân A4/A5
- Module BLE HM-10 — UART qua SoftwareSerial, chân D10 (RX) / D11 (TX)
- Màn hình OLED SSD1306 0.96" — I²C, chung bus với MLX90614
- 2 LED (xanh D5, đỏ D6), còi chip (D9), 2 nút nhấn (D2 chức năng, D3 reset)

Toàn bộ pin mapping và hằng số theo spec nằm trong [include/config.h](include/config.h) —
sửa số liệu ở đó, không sửa rải trong code.

## Build & nạp

```bash
pip3 install --user --break-system-packages platformio   # 1 lần
cd firmware
~/.local/bin/pio run              # build
~/.local/bin/pio run -t upload    # nạp vào board (cắm USB trước)
~/.local/bin/pio device monitor   # xem debug qua Serial USB (115200 baud)
```

Sau `pio run`, luôn đọc lại dòng `Flash: XX.X%` — spec bắt ≤75%. Nếu vượt sau
khi thêm code, cắt chuỗi debug hoặc bỏ tính năng thừa trước, đừng đổi board.

## Cấu trúc code — đi từ đâu tới đâu

```
src/main.cpp          orchestration: setup/loop, nút nhấn, watchdog
src/measurement.cpp    lọc trimmed-mean 8→6 mẫu, bù nhiệt K1/K2, phân loại 4 nhóm
src/mlx90614.cpp       driver cảm biến (đọc I2C, kiểm PEC, retry 3 lần)
src/eeprom_store.cpp   đọc/ghi threshold + K1/K2 vào EEPROM, có CRC8
src/indicators.cpp     LED/còi/OLED — toàn non-blocking (millis(), không delay())
src/ble_protocol.cpp   nhận lệnh cấu hình + gửi gói dữ liệu qua HM-10
src/crc8.cpp           CRC-8/MAXIM dùng chung cho PEC cảm biến + CRC EEPROM
```

`main.cpp::loop()` là nơi bắt đầu đọc để hiểu luồng chạy: mỗi vòng —
reset watchdog → đọc nút → đọc lệnh BLE → đo nếu được yêu cầu → cập nhật
LED/còi. Không có `delay()` nào trong `loop()` để không treo watchdog (2s)
và giữ độ trễ cảnh báo ≤200ms như spec yêu cầu.

## Đã làm đúng theo spec, đã build-verify

- Lọc nhiễu: 8 mẫu/400ms, bỏ min+max, trung bình 6 mẫu còn lại (mục 5)
- Từ chối kết quả nếu spread mẫu > 1.0°C (mục 5, use case "đo sai do nhiễu")
- Khóa đo khi môi trường ngoài 16–35°C (mục 5, use case "môi trường không đạt")
- Phân loại 4 nhóm đúng bảng mục 6.2 (nghi ngờ / bình thường / cảnh báo / lỗi)
- EEPROM: CRC8 bảo vệ, dùng mặc định khi hỏng, chỉ ghi khi giá trị đổi (mục 3.1/3.2)
- Watchdog 2s tự reset nếu loop treo (mục 5)
- Flash dưới 75% (51.8%), còn dư địa cho code hiệu chỉnh sau

## Chưa làm / phải tự bổ sung khi có phần cứng thật

- **K1/K2 mặc định là 1.0/0.0 (không bù gì cả).** Đây là chỗ CHẮC CHẮN phải
  chỉnh khi có cảm biến thật: đo song song với nhiệt kế y tế đã kiểm định,
  rồi gửi lệnh `CALIBRATE=<k1>,<k2>\r\n` qua BLE để lưu lại (xem giao thức dưới).
- **Chưa test I2C/BLE/OLED thật** — chỉ build-verify (compile sạch), chưa nạp
  vào board nào. Rất có thể cần chỉnh lại địa chỉ I2C hoặc chân SoftwareSerial
  nếu module thực tế khác datasheet giả định.
- **Giao thức lệnh BLE là tôi tự đặt ra** — file docx đặc tả không cho bảng
  lệnh cụ thể, nên đây là quy ước tạm, đủ dùng để test bằng app BLE có sẵn
  (nRF Connect / LightBlue), có thể đổi khi làm app điện thoại thật:

  ```
  Gửi xuống board (qua UART/BLE, kết thúc \r\n):
    SET_THRESH=37.8          -> OK  |  ERR:RANGE
    CALIBRATE=1.02,-0.3      -> OK  |  ERR:RANGE | ERR:FORMAT
    GET_STATUS               -> OK

  Board gửi lên (1 gói/giây hoặc ngay khi có kết quả mới):
    TEMP,<giá trị>,<NORMAL|ALARM|SUSPECT|ERROR>,<crc8 hex>
  ```

- **Không có app điện thoại** — dùng app BLE test có sẵn để thử giao thức
  trên trước khi build app riêng.
- **Không có PCB/vỏ 3D** — ngoài phạm vi code, xem mục 9 trong docx spec.

## Nếu build lỗi / Flash vượt 75%

- Lỗi "undeclared" thường là thiếu `#include "config.h"` trong file `.h` mới —
  đã gặp 1 lần ở `ble_protocol.h`, đã sửa.
- Warning `format '%f' expects double` là bình thường (AVR `snprintf` luôn
  promote `float`→`double` khi in) — không phải lỗi, không ảnh hưởng chạy.
- Nếu Flash vượt 75% sau khi thêm tính năng: kiểm tra có vô tình `#include`
  `Adafruit_GFX`/`Adafruit_SSD1306` không — dự án này dùng `U8x8lib.h`
  (chế độ text-only) để giữ Flash nhẹ, đừng đổi sang U8g2 full-graphics.
