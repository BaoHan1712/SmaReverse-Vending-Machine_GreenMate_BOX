from get_library import *
from toUart import *
import time
import threading
import queue
import cv2

class YOLOProcessor(threading.Thread):
    def __init__(self, video_path, model_path, output_queue):
        super().__init__(daemon=True)
        self.video_path = video_path
        self.model_path = model_path
        self.output_queue = output_queue
        self.running = True

        #--- Khởi tạo UART ---
        self.send_uart = ESP32_UART(port='/dev/ttyACM0', baudrate=9600)

        #--- Bộ đếm thời gian và các cấu hình ---
        self.last_send_time = 0
        self.send_interval = 5  # seconds between any sends
        self.min_detection_duration = 2 # seconds for stable detection
        self.track_start_times = {}  # Thời điểm track_id xuất hiện
        self.sent_ids = set()  # Các ID đã gửi

    def run(self):
        try:
            model = YOLO(self.model_path)
        except Exception as e:
            print(f"⚠️ Lỗi tải model: {e}")
            return

        try:
            cap = cv2.VideoCapture(self.video_path)
            if not cap.isOpened():
                raise IOError(f"Không thể mở video tại: {self.video_path}")
        except Exception as e:
            print(f"⚠️ Lỗi mở video: {e}")
            return

        total_label_0 = 0
        total_label_1 = 0

        while self.running and cap.isOpened():
            success, frame = cap.read()
            if not success:
                if isinstance(self.video_path, str):
                    print("⚠️ Hết video.")
                    break
                continue

            frame = cv2.resize(frame, (480, 320))
            results = model.track(
                source=frame, imgsz=480, conf=0.5,
                verbose=False, persist=True,
                tracker=r'tracking/bytetrack.yaml'
            )[0]

            if results.boxes and results.boxes.is_track:
                boxes = results.boxes.xywh.cpu().numpy()
                track_ids = results.boxes.id.int().cpu().tolist()
                cls_ids = results.boxes.cls.int().cpu().tolist()

                frame = results.plot(boxes=True, color_mode='instance')

                current_time = time.time()

                for box, track_id, cls_id in zip(boxes, track_ids, cls_ids):
                    x, y, w, h = box
                    center_x = int(x)
                    center_y = int(y)
                    cv2.circle(frame, (center_x, center_y), 3, (0, 255, 0), -1)

                    # Ghi lại thời gian xuất hiện của track_id
                    if track_id not in self.track_start_times:
                        self.track_start_times[track_id] = current_time

                    # Tính thời gian tồn tại của track_id
                    time_tracked = current_time - self.track_start_times[track_id]

                    # Kiểm tra điều kiện: tồn tại >=2s, chưa gửi, và cách lần gửi trước >=5s
                    if (
                        time_tracked >= self.min_detection_duration and
                        track_id not in self.sent_ids and
                        (current_time - self.last_send_time) >= self.send_interval
                    ):
                        self.sent_ids.add(track_id)
                        self.last_send_time = current_time

                        if cls_id == 0:
                            total_label_0 += 1
                            self.send_uart.send_packet(1)
                        elif cls_id == 1:
                            total_label_1 += 1
                            self.send_uart.send_packet(2)

                        # 🔑 XÓA HẾT DANH SÁCH ID CŨ
                        self.sent_ids.clear()

            cv2.putText(frame, f"bottle: {total_label_0}", (20, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 0, 0), 2)
            cv2.putText(frame, f"can: {total_label_1}", (20, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

            try:
                self.output_queue.put_nowait((frame, total_label_0, total_label_1))
            except queue.Full:
                pass

        cap.release()
        print("Luồng YOLO đã dừng.")

    def stop(self):
        self.running = False


# ===============================================================
# CLASS IN PHIẾU
# ===============================================================
import datetime
import platform
import subprocess
import tempfile
import os

IS_WINDOWS = platform.system() == "Windows"
IS_LINUX = platform.system() == "Linux"

if IS_WINDOWS:
    try:
        import win32print
    except ImportError:
        IS_WINDOWS = False
        print("Cảnh báo: Thư viện 'pywin32' không được tìm thấy. Chức năng in trên Windows sẽ không hoạt động.")
        print("Để cài đặt, chạy lệnh: pip install pywin32")


class ReceiptPrinter:
    """
    Class in phiếu tương thích với Windows và Linux (Raspberry Pi).
    """
    def __init__(self):
        self.is_ready = IS_WINDOWS or IS_LINUX
        if not self.is_ready:
            print("Cảnh báo: Hệ điều hành không được hỗ trợ để in.")

    def print_receipt(self, user_name, bottles, cans, points):
        if not self.is_ready:
            return False, "Chuc nang in không khả dung trên he đieu hành này."

        try:
            now = datetime.datetime.now()
            date_str = now.strftime("%d/%m/%Y")
            time_str = now.strftime("%H:%M:%S")

            receipt_content = (
                "   PHIEU TICH DIEM TAI CHE\n"
                "--------------------------------\n"
                f"Khach hang: {user_name}\n"
                f"Ngay: {date_str}\n"
                f"Gio: {time_str}\n"
                "--------------------------------\n"
                "So luong vat pham:\n"
                f"- Chai nhua:      {bottles}\n"
                f"- Lon kim loai:   {cans}\n"
                f"Tong diem:        {points}\n"
                "--------------------------------\n"
                "Cam on ban da chung tay bao ve\n"
                "         moi truong!\n\n\n"
            )

            if IS_WINDOWS:
                return self._print_windows(receipt_content, user_name)
            elif IS_LINUX:
                return self._print_linux(receipt_content, user_name)

        except Exception as e:
            error_message = f"Không thể in phiếu. Đã xảy ra lỗi:\n{e}"
            print(error_message)
            return False, error_message

    def _print_windows(self, content, user_name):
        try:
            printer_name = win32print.GetDefaultPrinter()
            h_printer = win32print.OpenPrinter(printer_name)
            try:
                h_job = win32print.StartDocPrinter(h_printer, 1, ("Phieu Tich Diem", None, "RAW"))
                try:
                    win32print.StartPagePrinter(h_printer)
                    win32print.WritePrinter(h_printer, content.encode('utf-8'))
                    win32print.EndPagePrinter(h_printer)
                finally:
                    win32print.EndDocPrinter(h_printer)
            finally:
                win32print.ClosePrinter(h_printer)

            success_message = f"Send '{user_name}' succes (Windows)."
            print(success_message)
            return True, success_message

        except Exception as e:
            return False, f"Lỗi khi in trên Windows: {e}"

    def _print_linux(self, content, user_name):
        try:
            with tempfile.NamedTemporaryFile(mode='w+', delete=False) as temp_file:
                temp_file.write(content)
                temp_file_path = temp_file.name

            # Gửi nội dung đến máy in mặc định qua CUPS
            subprocess.run(['lp', temp_file_path], check=True)

            os.remove(temp_file_path)  # Dọn dẹp file tạm

            success_message = f"Send '{user_name}' succes (Linux)."
            print(success_message)
            return True, success_message

        except subprocess.CalledProcessError as e:
            return False, f"Loi khi gui lenh in qua CUPS: {e}"
        except Exception as e:
            return False, f"Loi không xac đinh khi in trên Linux: {e}"
