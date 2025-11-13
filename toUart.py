import serial
import struct
import time

class ESP32_UART:
    def __init__(self, port, baudrate=9600):
        try:
            self.ser = serial.Serial(port, baudrate)
            print(f"✅ Đã kết nối thành công UART")
        except serial.SerialException as e:
            self.ser = None
            print(f"❌ Lỗi: Không thể mở cổng {port}. {e}")

    def send_packet(self, data_byte):
        if not self.ser or not self.ser.is_open:
            print("Lỗi: Kết nối serial chưa được thiết lập hoặc đã đóng.")
            return
        try:
            packet = struct.pack('<BBB', 0x02, data_byte, 0x03)
            self.ser.write(packet)
            print(f"Đã gửi Data: {hex(data_byte)}")
        except Exception as e:
            print(f"Lỗi khi gửi dữ liệu: {e}")

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✅ Đã đóng kết nối serial.")

    def __del__(self):
        self.close()


esp32 = ESP32_UART(port='COM6', baudrate=9600)
last_send_time = 0
interval = 5  # giây

while True:
    current_time = time.time()
    if current_time - last_send_time >= interval:
        if esp32.ser:
            esp32.send_packet(5)
        last_send_time = current_time
    # Nếu sau này muốn thêm xử lý khác thì vẫn chạy mượt
