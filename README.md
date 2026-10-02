# Giải Mã Tín Hiệu SIF Votol & Chuyển Đổi CAN Bus Cho Màn Hình VinFast Evo200

Dự án này cung cấp giải pháp độ chế/thay thế IC điều tốc (Controller Votol) cho dòng xe máy điện **VinFast Evo200** mà vẫn giữ nguyên màn hình hiển thị nguyên bản (HMI / Đồng hồ xe) hoạt động mượt mà. 

Mạch trung gian sử dụng vi điều khiển **STM32F103C8T6 (Blue Pill)** kết hợp với module thu phát CAN **MCP2515** để giải mã chuỗi xung SIF 1 dây (One-Wire) từ IC Votol và đóng gói gửi dữ liệu chuẩn lên mạng CAN Bus 500Kbps của VinFast.

---

## 📌 Tính Năng Nổi Bật

- **Đồng bộ tốc độ chuẩn xác:** Chuyển đổi RPM từ Votol sang tốc độ hiển thị km/h, không bị giật lag hay trễ tay ga.
- **Xử lý vị trí số linh hoạt:** Tự động hiển thị chính xác các trạng thái số **P** (Đỗ), **D** (Số tiến / Eco), **S** (Sport), **R** (Số lùi).
- **Hỗ trợ phanh:** Đồng bộ trạng thái phanh đĩa/phanh điện từ điều tốc lên đồng hồ.
- **Tối ưu hóa băng thông CAN:** Sử dụng cơ chế gửi gói đồng thời (chu kỳ 20ms) giúp chống nghẽn hàng đợi buffer và giữ Alive Counter liên tục.

---

## 🛠️ Sơ Đồ Kết Nối Phần Cứng (Hardware Wiring)

### 1. STM32F103C8T6 $\leftrightarrow$ Module MCP2515 (Giao tiếp SPI1 Parts)

| Mạch MCP2515 | STM32F103C8T6 (Blue Pill) | Ghi Chú |
| :--- | :--- | :--- |
| **VCC** | **5V** | **Bắt buộc dùng 5V** để cấp nguồn cho IC thu phát TJA1050 |
| **GND** | **GND** | Nối chung Mass toàn hệ thống |
| **CS (NSS)** | **PA4** | SPI1 Chip Select |
| **SCK** | **PA5** | SPI1 Clock |
| **SO (MISO)**| **PA6** | SPI1 MISO |
| **SI (MOSI)**| **PA7** | SPI1 MOSI |

> ⚠️ **Lưu ý quan trọng:** Cần nối chung chân **GND** giữa IC Votol, Mạch STM32, Module MCP2515 và dây Âm nguồn của xe VinFast.

### 2. Tín Hiệu SIF & Debug

- **Dây SIF từ IC Votol:** Đấu vào chân **PA0** trên STM32 (Đọc ngắt tần số xung).
- **Cổng Debug UART1:** **TX = PA10**, **RX = PA9** (Giao tiếp qua mạch chuyển đổi USB-to-TTL với Baudrate `115200`).

---

## 🔍 Tổng Kết Các Vấn Đề Kỹ Thuật Cốt Lõi (Troubleshooting Log)

### 1. Tốc độ Baudrate & Thạch Anh (Crystal)
- Xe VinFast Evo200 chạy chuẩn giao tiếp CAN **500 Kbps** (các dòng KlaraS/Ludo/Impes đời cũ chạy 250 Kbps).
- Đa số các mạch MCP2515 trên thị trường dùng thạch anh **16MHz**. Cần khai báo đúng `MCP_16MHZ` trong code, nếu chọn sai thành `8MHz`, tần số thực tế sẽ bị nhân đôi gây lỗi giao tiếp.

### 2. Bộ Đếm Alive Counter (Byte 6)
- Màn hình VinFast bắt buộc các gói CAN (`0x181`, `0x201`, `0x202`) phải có biến đếm liên tục từ `0 - 255` ở Byte 6.
- **Không dùng chung 1 biến đếm cho nhiều ID**, phải tách biệt thành các biến độc lập: `counter_181`, `counter_201`, `counter_202`.

### 3. Định Dạng Byte Tốc Độ (Endiness & Speed Scaling)
- Đồng hồ VinFast đọc giá trị vận tốc theo định dạng **Big-Endian / MSB First** (Byte cao gửi trước, Byte thấp gửi sau):
  ```cpp
  msg.data[0] = speed_to_hmi >> 8;   // Byte cao
  msg.data[1] = speed_to_hmi & 0xFF;  // Byte thấp
