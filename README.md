Tài Liệu Kỹ Thuật: Giải Mã SIF Votol & Chuyển Đổi CAN Bus Cho Xe Điện VinFast (Evo200)Dự án này giải quyết bài toán thay thế hoặc độ chế IC điều khiển (Controller Votol) trên các dòng xe máy điện VinFast mà vẫn giữ nguyên màn hình hiển thị nguyên bản (HMI / Đồng hồ xe) thông qua việc giải mã giao thức SIF 1 dây và giao tiếp CAN Bus.1. Sơ Đồ Kết Nối Phần Cứng (Hardware Wiring)STM32F103C8T6 $\leftrightarrow$ Module MCP2515 (SPI1 phần cứng)VCC: Nối với chân 5V trên STM32 (Lưu ý: IC thu phát TJA1050 trên mạch MCP2515 bắt buộc dùng nguồn 5V để đảm bảo biên độ tín hiệu CAN).GND: Nối GND chung toàn hệ thống (STM32, MCP2515, Votol, Xe VinFast, Mạch TTL).CS (NSS): Chân PA4SCK: Chân PA5SO (MISO): Chân PA6SI (MOSI): Chân PA7Tín hiệu SIF & Giao tiếp UARTSIF Pin (Input): Chân PA0 (Đọc tín hiệu xung One-Wire từ IC Votol).Debug Serial (UART1): TX = PA9, RX = PA10 (Kết nối qua mạch USB-to-TTL với tốc độ baudrate 115200).2. Các Vấn Đề Kỹ Thuật Quan Trọng & Trải Nghiệm Thực Tế (Troubleshooting Log)A. Tốc độ Baudrate Mạng CAN (Baudrate Setting)Các dòng xe VinFast thế hệ cũ (KlaraS, Ludo, Impes): Chạy ở tốc độ 250 Kbps.Dòng xe VinFast Evo200: Chạy ở tốc độ 500 Kbps.Lưu ý về Thạch Anh (Crystal): Hầu hết các mạch MCP2515 trên thị trường hiện nay sử dụng thạch anh 16MHz. Cần khai báo đúng MCP_16MHZ trong thư viện (nếu chọn sai thành MCP_8MHZ, tốc độ baudrate thực tế sẽ bị sai gấp đôi và màn hình hoàn toàn "im lặng").B. Bộ Đếm Alive Counter (Tránh Hiện Tượng Drop Frame)Màn hình VinFast kiểm tra tính liên tục của dữ liệu nhận được bằng bộ đếm Alive Counter ở Byte 6 của các khung CAN (0x181, 0x201, 0x202).Sạn kỹ thuật: Không dùng chung một biến counter++ cho tất cả các ID gói tin trong cùng một vòng lặp. Việc dùng chung sẽ làm biến đếm bị nhảy cóc (tăng 2-3 đơn vị mỗi chu kỳ), làm HMI nghi ngờ mất gói tin và từ chối hiển thị.Giải pháp: Tách riêng từng biến đếm độc lập: counter_181, counter_201, counter_202.C. Định Dạng Dữ Liệu Tốc Độ & Thứ Tự Byte (Endiness & Speed Mapping)Thứ tự Byte (Endiness): Đồng hồ VinFast đọc giá trị vận tốc ở dạng Big-Endiness / MSB First (Byte cao gửi trước, Byte thấp gửi sau).msg.data[0] = speed_to_hmi >> 8; (Byte cao)msg.data[1] = speed_to_hmi & 0xFF; (Byte thấp)Lưu ý: Nếu bị ngược Byte, đồng hồ sẽ bị hiện tượng nhảy vọt lên max tốc 199 km/h ngay khi vừa nhích nhẹ tay ga.Tỷ lệ chia RPM sang Vận tốc (Speed Scaling Factor):Giá trị RPM trả về qua chuỗi SIF của Votol rất lớn tùy thuộc vào số cặp cực từ của động cơ.Tỷ lệ tối ưu được thử nghiệm chuẩn xác trên bánh xe VinFast Evo200 là:$$\text{Speed\_To\_HMI} = \frac{\vert{}\text{RPM}\vert{}}{12}$$(Nếu thấy tốc độ báo thấp hơn thực tế thì giảm hệ số này xuống 10 - 11; nếu cao hơn thì tăng lên 13 - 15).D. Khắc Phục Trễ Tay Ga (Latency / Lag Speed On Display)Lỗi thiết kế: Nếu chia các gói CAN ra gửi tuần tự theo từng giai đoạn (switch-case giãn cách 5ms), tổng thời gian hoàn thành 1 lượt gửi sẽ kéo dài trên 20ms - 40ms, gây ra độ trễ sâu (lag) khi tăng/giảm ga trên màn hình.Giải pháp tối ưu: Bỏ switch-case. Đóng gói và gửi liên tiếp toàn bộ các ID CAN (0x181, 0x201, 0x202, 0x208) trong cùng một chu kỳ định thời 20ms (chỉ chèn thêm delayMicroseconds(200) giữa các lần bắn gói để tránh nghẽn buffer MCP2515).E. Đồng Bộ Trạng Thái Số Lùi (R), Phanh & Chế Độ Lái (Drive Mode)Dữ liệu ký tự hiển thị số trên màn hình được điều khiển chính bởi gói 0x201:Số lùi (R): Byte data[3] = 0x02 (khi biến car_reverse == true).Số đỗ (P): Byte data[3] = 0x00 (khi xe đứng im car_rpm == 0 và không bóp phanh).Số tiến (D/S - Eco/Sport): Byte data[3] = 0x01 (Chế độ D) hoặc 0x03 (Chế độ S tùy theo biến car_driveMode nhận từ SIF).3. Mã Nguồn Hoàn Chỉnh (STM32F103 - Arduino Framework)Code dưới đây đã được gọt dũa tối ưu hiệu năng, cắt bỏ toàn bộ comment để sẵn sàng biên dịch:C++#include <SPI.h>
#include <mcp2515.h>

#define MySerial Serial1

const uint8_t SIF_PIN = PA0;  
const uint8_t CAN_CS  = PA4;  

MCP2515 mcp2515(CAN_CS);

volatile uint8_t data[12] = {0};
volatile int16_t bitIndex = -1; 
volatile uint32_t lastTime = 0;
volatile uint32_t lastDuration = 0;
volatile bool msgReady = false; 

int16_t car_rpm = 0;
bool car_brake = false;
bool car_reverse = false;
uint8_t car_driveMode = 0;

uint32_t lastCAN = 0;
uint32_t last101 = 0; 

struct can_frame msg101;
struct can_frame msg181; 
struct can_frame msg201;
struct can_frame msg202; 
struct can_frame msg208;

uint8_t counter_181 = 0;
uint8_t counter_201 = 0;
uint8_t counter_202 = 0;

void sifISR(void);

void setup() {
  MySerial.begin(115200); 

  pinMode(SIF_PIN, INPUT_PULLUP); 
  lastTime = micros();
  attachInterrupt(digitalPinToInterrupt(SIF_PIN), sifISR, CHANGE);

  SPI.begin(); 
  mcp2515.reset();

  if (mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ) == MCP2515::ERROR_OK) {
    MySerial.println("CAN Driver (MCP2515): 500K OK");
  } else {
    MySerial.println("CAN Driver: KHOR TAI THAT BAI!");
  }

  mcp2515.setNormalMode();
  MySerial.println("--- STM32 CAN 500K READY ---");
}

void loop() {
  uint32_t now = millis();

  if (msgReady) {
    uint8_t localData[12];
    
    noInterrupts(); 
    memcpy(localData, (const uint8_t*)data, 12);
    msgReady = false;
    interrupts();

    uint8_t crc = 0;
    for (int i = 0; i < 11; i++) {
      crc ^= localData[i];
    }

    if (crc == localData[11]) {
      uint8_t status = localData[4];
      car_brake     = (status >> 5) & 1;
      car_reverse   = (localData[5] >> 2) & 1;
      car_driveMode = status & 0x07;
      car_rpm       = (int16_t)((localData[7] << 8) | localData[8]);
    }
  }

  if (now - last101 >= 100) {
    last101 = now;

    msg101.can_id = 0x101;
    msg101.can_dlc = 3;
    msg101.data[0] = 0x11;
    msg101.data[1] = 0x10;
    msg101.data[2] = 0x01;

    mcp2515.sendMessage(&msg101);
  }

  if (now - lastCAN >= 20) {
    lastCAN = now;
    
    uint16_t speed_to_hmi = abs(car_rpm) / 12; 

    // Gói 0x181
    msg181.can_id = 0x181;
    msg181.can_dlc = 8;
    msg181.data[0] = speed_to_hmi >> 8;
    msg181.data[1] = speed_to_hmi & 0xFF;
    msg181.data[2] = 0x00;
    msg181.data[3] = 0x00;
    msg181.data[4] = 0x00;
    msg181.data[5] = 0x00;
    msg181.data[6] = counter_181++;
    msg181.data[7] = 0x00;
    mcp2515.sendMessage(&msg181);
    delayMicroseconds(200);

    // Gói 0x201 (Xử lý vị trí số P, D, R, S)
    uint8_t gear_send = 0x01;
    if (car_reverse) {
      gear_send = 0x02;
    } else if (car_rpm == 0 && !car_brake) {
      gear_send = 0x00;
    } else {
      gear_send = (car_driveMode == 2) ? 0x03 : 0x00;
    }

    msg201.can_id  = 0x201;
    msg201.can_dlc = 8;
    msg201.data[0] = speed_to_hmi >> 8;
    msg201.data[1] = speed_to_hmi & 0xFF;
    msg201.data[2] = car_reverse ? 0x01 : 0x00;
    msg201.data[3] = gear_send;
    msg201.data[4] = 0x00;
    msg201.data[5] = 0x00;
    msg201.data[6] = counter_201++;
    msg201.data[7] = car_brake ? 0x01 : 0x00;
    mcp2515.sendMessage(&msg201);
    delayMicroseconds(200);

    // Gói 0x202
    msg202.can_id = 0x202;
    msg202.can_dlc = 8;
    msg202.data[0] = speed_to_hmi >> 8;
    msg202.data[1] = speed_to_hmi & 0xFF;
    msg202.data[2] = 0x00;
    msg202.data[3] = 0x00;
    msg202.data[4] = 0x00;
    msg202.data[5] = 0x00;
    msg202.data[6] = counter_202++;
    msg202.data[7] = 0x00;
    mcp2515.sendMessage(&msg202);
    delayMicroseconds(200);

    // Gói 0x208
    msg208.can_id = 0x208;
    msg208.can_dlc = 8;
    msg208.data[0] = 0x00;
    msg208.data[1] = car_brake ? 0x01 : 0x00; 
    msg208.data[2] = 0x01;
    msg208.data[3] = 0x77;
    msg208.data[4] = 0x00;
    msg208.data[5] = 0x00;
    msg208.data[6] = 0x70;
    msg208.data[7] = 0x32;
    mcp2515.sendMessage(&msg208);
  }

  static uint32_t lastDbg = 0;
  if (now - lastDbg > 500) {
    lastDbg = now;
    uint16_t speed_to_hmi = abs(car_rpm) / 12;
    
    MySerial.printf(
      "RPM=%d Speed=%u Brake=%d Rev=%d Mode=%d\n",
      car_rpm,
      speed_to_hmi,
      car_brake,
      car_reverse,
      car_driveMode
    );
  }
}

void sifISR(void) {
  uint32_t now = micros();
  uint32_t duration = now - lastTime;
  lastTime = now;

  if (digitalRead(SIF_PIN) == LOW) {
    if (lastDuration == 0) {
      lastDuration = duration;
      return;
    }

    if (lastDuration > 2000 && lastDuration > (duration * 15)) {
      bitIndex = 0;
      lastDuration = duration;
      return;
    }

    if (bitIndex >= 0 && bitIndex < 96) {
      uint8_t b = bitIndex >> 3;
      uint8_t p = 7 - (bitIndex & 7);

      if (lastDuration > duration) {
        data[b] &= ~(1 << p);
        bitIndex++;
      } 
      else if (duration > lastDuration) {
        data[b] |= (1 << p);
        bitIndex++;
      }

      if (bitIndex >= 96) {
        bitIndex = -1; 
        msgReady = true; 
      }
    }
  }
  lastDuration = duration;
}
