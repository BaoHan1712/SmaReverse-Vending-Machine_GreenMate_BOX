#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

const byte START_BYTE = 0x02;
const byte END_BYTE   = 0x03;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50     // 50Hz cho servo TD-8120MG

// Dải xung cho servo 180
#define SERVOMIN_US 500   // ~0°
#define SERVOMAX_US 2500  // ~180°

// Dải xung cho servo 360 độ 
#define SERVO_360_STOP_US 1500  // Dừng
#define SERVO_360_LEFT_US 1300  // Quay trái
#define SERVO_360_RIGHT_US 1700 // Quay phải

void setup() {
  Serial.begin(9600);
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  delay(1000);
  rotate_default();

}

// ---- Hàm chuyển microsecond -> tick 12-bit ----
uint16_t microsecondsToTicks(int microseconds) {
  float tick = (microseconds * 4096.0) / 20000.0;
  return (uint16_t)tick;
}

// ---- Hàm chuyển góc sang tick ----
uint16_t angleToPulse(int angle) {
  int us = map(angle, 0, 180, SERVOMIN_US, SERVOMAX_US);
  return microsecondsToTicks(us);
}

/**
 * @param angle   Góc cần xoay (0–180)
 * @param channel Kênh servo
 */
void moveServo180(int angle, int channel) {
  uint16_t pulse = angleToPulse(angle);
  pwm.setPWM(channel, 0, pulse);
  delay(1000);
}

/**
 * @brief Điều khiển servo 360° (continuous rotation)
 * @param speed -100 đến 100 (%), âm = quay trái, dương = quay phải, 0 = dừng
 * @param channel Kênh servo
 */
void moveServo360(int speed, int channel) {
  // Giới hạn tốc độ
  speed = constrain(speed, -100, 100);

  int us = SERVO_360_STOP_US;

  if (speed > 0) {
    us = map(speed, 0, 100, SERVO_360_STOP_US, SERVO_360_RIGHT_US);
  } else if (speed < 0) {
    us = map(speed, -100, 0, SERVO_360_LEFT_US, SERVO_360_STOP_US);
  }

  uint16_t pulse = microsecondsToTicks(us);
  pwm.setPWM(channel, 0, pulse);
}

/**
 * @brief Dừng servo 360°
 */
void stopServo360(int channel) {
  uint16_t pulse = microsecondsToTicks(SERVO_360_STOP_US);
  pwm.setPWM(channel, 0, pulse);
}

/**
 * @param speed    -100 đến 100 (%), âm = quay trái, dương = quay phải
 * @param channel  Kênh servo
 * @param duration Thời gian quay (ms)
 */
void moveServo360ForTime(int speed, int channel, unsigned long duration) {
  moveServo360(speed, channel);      // Bắt đầu quay
  delay(duration);                   // Giữ xung trong thời gian duration
  stopServo360(channel);             // Dừng servo sau khi hết thời gian
}

/// -------------------------- Nhận tín hiệu để xoay ---------------------------------
// Nhận mảng 3 byte từ UART: [START, DATA, END]
void receive_data() {
  const int PACKET_SIZE = 3;
  byte buffer[PACKET_SIZE];

  while (Serial.available() >= PACKET_SIZE) {
    Serial.readBytes(buffer, PACKET_SIZE);

    if (buffer[0] == START_BYTE && buffer[2] == END_BYTE) {
      handle_command(buffer[1]);  // xử lý byte dữ liệu ở giữa
    } else {
      // Nếu không đúng định dạng, bỏ qua
      Serial.println("Gói không hợp lệ");
    }
  }
}

// Xử lý servo theo lệnh nhận
void handle_command(byte command) {
  switch (command) {
    case 0x01:
    classify_bottle();
      break;

    case 0x02:
    classify_can();
      break;

    default:
      // Lệnh không hợp lệ
      // Serial.println("Lệnh không hỗ trợ.");
      break;
  }
}
// ham xoay servo ve goc ban dau
void rotate_default () {
  moveServo180(125, 15);
  moveServo180(90, 14);
}

// hàm phân loại chai
void classify_bottle () {
  moveServo180(180, 14); // quay sang trái
  moveServo180(20,15); // nga mang xuong
  rotate_default();
}

// hàm phân loại lon
void classify_can () {
    moveServo180(0,14); // quay sang phai
    moveServo180(20,15); // nga mang xuong
    rotate_default();
}

// Hàm test 5 servo
void test_servo_180() {
  //// xoay servo 180
  classify_bottle();
  classify_can();
  // delay(1000);

}

void test_servo_360() {
    
//   //// xoay servo 360
  moveServo360ForTime(-100, 0, 2300);
  moveServo360ForTime(-100, 1, 2300);
  moveServo360ForTime(-100, 2, 2300);
}

void loop() {

  receive_data();
  // test_servo_180();
  // test_servo_360();

  
}
