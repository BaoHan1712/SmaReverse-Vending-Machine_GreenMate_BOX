import serial
import struct
import time

class ESP32_UART:
    """
    Lớp để giao tiếp với ESP32 qua kết nối UART (Serial).
    """
    def __init__(self, port, baudrate=9600):
        try:
            self.ser = serial.Serial(port, baudrate)
            print(f"✅ Đã kết nối thành công uart")
        except serial.SerialException as e:
            self.ser = None
            print(f"❌ Lỗi: Không thể mở cổng {port}. {e}")

    def send_packet(self, data_byte):
        """
        - Byte bắt đầu: 0x02
        - Data: 1 byte
        - Byte kết thúc: 0x03
        """
        if not self.ser or not self.ser.is_open:
            print("Lỗi: Kết nối serial chưa được thiết lập hoặc đã đóng.")
            return
        try:
            start_byte = 0x02
            end_byte = 0x03
            packet = struct.pack('<BBB', start_byte, data_byte, end_byte)
            self.ser.write(packet)
            print(f"Đã gửi Data: {hex(data_byte)}")
            
        except Exception as e:
            print(f"Lỗi khi gửi dữ liệu: {e}")
            
    def close(self):
        """
        Đóng kết nối serial.
        """
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✅ Đã đóng kết nối serial.")
            
    def __del__(self):
        """
        Hàm hủy tự động đóng kết nối khi đối tượng bị xóa.
        """
        self.close()


esp32 = ESP32_UART(port='COM6', baudrate=9600)
while True:
    if esp32.ser: # Chỉ chạy nếu kết nối thành công
        # Gửi giá trị
        esp32.send_packet(1)
        time.sleep(1)
