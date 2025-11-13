#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

const byte START_BYTE = 0x02;
const byte END_BYTE   = 0x03;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_FREQ 50     // 50Hz cho servo TD-8120MG

// Dải xung cho servo 180
#define SERVOMIN_US 500
#define SERVOMAX_US 2500

// Dải xung cho servo 360 độ
#define SERVO_360_STOP_US 1500
#define SERVO_360_LEFT_US 1300
#define SERVO_360_RIGHT_US 1700

// Thời gian xoay servo 180 (ms)
#define SERVO_180_TIME 1000

// ========================= Biến điều khiển xoay 180 tuần tự =========================
struct ServoStep {
  int angle;
  int channel;
};

ServoStep sequence[10];
int stepCount = 0;
int currentStep = 0;
unsigned long stepStartMillis = 0;
bool servoBusy = false;

// ========================= Setup =========================
void setup() {
  Serial.begin(9600);
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  delay(1000);

  rotate_default();
}

// ========================= Chuyển đổi =========================
uint16_t microsecondsToTicks(int microseconds) {
  float tick = (microseconds * 4096.0) / 20000.0;
  return (uint16_t)tick;
}

uint16_t angleToPulse(int angle) {
  int us = map(angle, 0, 180, SERVOMIN_US, SERVOMAX_US);
  return microsecondsToTicks(us);
}

// ========================= Hàm xoay servo 180 theo millis =========================
void moveServo180Timed(int angle, int channel) {
  uint16_t pulse = angleToPulse(angle);
  pwm.setPWM(channel, 0, pulse);

  stepStartMillis = millis();
  servoBusy = true;
}

bool updateServo180() {
  if (servoBusy && millis() - stepStartMillis >= SERVO_180_TIME) {
    servoBusy = false;
    return true; // xoay xong
  }
  return false; // đang xoay
}

// ========================= Hàm xoay 360 =========================
// ========================= Biến điều khiển servo 360 theo thời gian =========================
struct Servo360Timed {
  int speed;
  int channel;
  unsigned long duration;
  unsigned long startMillis;
  bool running;
};

Servo360Timed servo360;

// ========================= Hàm bắt đầu xoay servo 360 theo thời gian =========================
void moveServo360Timed(int speed, int channel, unsigned long duration) {
  speed = constrain(speed, -100, 100);
  int us = SERVO_360_STOP_US;
  if (speed > 0) us = map(speed, 0, 100, SERVO_360_STOP_US, SERVO_360_RIGHT_US);
  else if (speed < 0) us = map(speed, -100, 0, SERVO_360_LEFT_US, SERVO_360_STOP_US);

  pwm.setPWM(channel, 0, microsecondsToTicks(us));

  servo360.speed = speed;
  servo360.channel = channel;
  servo360.duration = duration;
  servo360.startMillis = millis();
  servo360.running = true;
}

// ========================= Hàm update servo 360 theo millis =========================
void updateServo360() {
  if (servo360.running && millis() - servo360.startMillis >= servo360.duration) {
    pwm.setPWM(servo360.channel, 0, microsecondsToTicks(SERVO_360_STOP_US));
    servo360.running = false;
  }
}


void moveServo360(int speed, int channel) {
  speed = constrain(speed, -100, 100);
  int us = SERVO_360_STOP_US;
  if (speed > 0) us = map(speed, 0, 100, SERVO_360_STOP_US, SERVO_360_RIGHT_US);
  else if (speed < 0) us = map(speed, -100, 0, SERVO_360_LEFT_US, SERVO_360_STOP_US);
  pwm.setPWM(channel, 0, microsecondsToTicks(us));
}

void stopServo360(int channel) {
  pwm.setPWM(channel, 0, microsecondsToTicks(SERVO_360_STOP_US));
}

// ========================= Hàm thêm bước xoay vào chuỗi =========================
void addServoStep(int angle, int channel) {
  if (stepCount < 10) {
    sequence[stepCount].angle = angle;
    sequence[stepCount].channel = channel;
    stepCount++;
  }
}

// ========================= Hàm xoay mặc định =========================
void rotate_default() {
  stepCount = 0;
  addServoStep(125, 15);
  addServoStep(90, 14);
  currentStep = 0;
}

// ========================= Hàm phân loại =========================
void classify_bottle() {
  stepCount = 0;
  addServoStep(180, 14); // quay trái
  addServoStep(20, 15);  // ngả máng xuống
  addServoStep(125, 15); // về mặc định
  addServoStep(90, 14);  // về mặc định
  currentStep = 0;
}

void classify_can() {
  stepCount = 0;
  addServoStep(0, 14);   // quay phải
  addServoStep(20, 15);  // ngả máng xuống
  addServoStep(125, 15); // về mặc định
  addServoStep(90, 14);  // về mặc định
  currentStep = 0;
}

// ========================= Loop =========================
void loop() {
  // Nhận dữ liệu UART
  receive_data();

  // Xử lý chuỗi servo 180 tuần tự
  if (currentStep < stepCount && !servoBusy) {
    moveServo180Timed(sequence[currentStep].angle, sequence[currentStep].channel);
    currentStep++;
  }
  updateServo180();

  // Xử lý servo 360 theo thời gian
  updateServo360();

  // // Ví dụ: khởi động servo 360 quay 2 giây nếu chưa chạy
  // if (!servo360.running) {
  //   moveServo360Timed(-80, 0, 2000); // speed = 80, channel = 0, duration = 2000ms
  // }
}

// ========================= Nhận dữ liệu UART =========================
void receive_data() {
  const int PACKET_SIZE = 3;
  byte buffer[PACKET_SIZE];

  while (Serial.available() >= PACKET_SIZE) {
    Serial.readBytes(buffer, PACKET_SIZE);
    if (buffer[0] == START_BYTE && buffer[2] == END_BYTE) {
      handle_command(buffer[1]);
    } 
  }
}

// ========================= Xử lý lệnh =========================
void handle_command(byte command) {
  switch (command) {
    case 0x01: classify_bottle(); break;
    case 0x02: classify_can(); break;
    case 0x03:  moveServo360Timed(-100, 0, 2300); break;
    case 0x04:  moveServo360Timed(-100, 1, 2300); break;
    case 0x05:  moveServo360Timed(-100, 2, 2300); break;

    default: break;
  }
}
