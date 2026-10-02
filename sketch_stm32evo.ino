#include <SPI.h>
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
    MySerial.println("CAN Driver: KHỞI TẠO THẤT BẠI! Kiem tra thach anh hoac day SPI.");
  }

  mcp2515.setNormalMode();
  MySerial.println("--- STM32 TTL SERIAL (UART1) & CAN 500K RUNNING ---");
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
    
    // GIẢM HỆ SỐ CHIA XUỐNG 12 ĐỂ TỐC ĐỘ HIỂN THỊ TĂNG LÊN
    uint16_t speed_to_hmi = abs(car_rpm) / 10; 

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

    msg201.can_id  = 0x201;
    msg201.can_dlc = 8;
    msg201.data[0] = speed_to_hmi >> 8;
    msg201.data[1] = speed_to_hmi & 0xFF;
    msg201.data[2] = car_reverse ? 0x01 : 0x00;
    msg201.data[3] = 0x05;
    msg201.data[4] = 0x00;
    msg201.data[5] = 0x00;
    msg201.data[6] = counter_201++;
    msg201.data[7] = car_brake ? 0x01 : 0x00;
    mcp2515.sendMessage(&msg201);
    delayMicroseconds(200);

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