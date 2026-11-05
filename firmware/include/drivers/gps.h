#ifndef GPS_H
#define GPS_H

#include <Arduino.h>


// ============================================================
// GPS DRIVER - NEO-7M
// ============================================================
//
// Nhiệm vụ:
//
// NEO-7M
//    ↓ UART
// ESP32-S3
//    ↓
// TinyGPS++
//    ↓
// Latitude
// Longitude
// Speed
// Satellites
//
// Sau đó dữ liệu sẽ được gửi:
//
// GPS
//  ↓
// main.cpp
//  ↓
// gsm.cpp
//  ↓
// A7680C / 4G
//  ↓
// POST /api/gps
//  ↓
// Azure
//  ↓
// PostgreSQL gps_logs
//
// ============================================================


// ============================================================
// INITIALIZE GPS
// ============================================================
//
// Khởi tạo UART dùng cho module NEO-7M.
//
// UART hiện tại:
//
// GPS TX -> ESP32 GPS_RX_PIN
// GPS RX -> ESP32 GPS_TX_PIN
//
// Baud rate lấy từ:
//
// GPS_BAUD
//
// trong config/settings.h.
//
// Return:
//
// true  = UART GPS đã được khởi tạo.
// false = khởi tạo thất bại.
//
// Lưu ý:
// true KHÔNG có nghĩa là GPS đã bắt được vệ tinh.
//
// ============================================================

bool initGPS();


// ============================================================
// READ GPS
// ============================================================
//
// Đọc dữ liệu NMEA từ NEO-7M trong khoảng timeoutMs.
//
// TinyGPS++ sẽ parse dữ liệu để lấy:
//
// - Latitude
// - Longitude
// - Speed
// - Satellites
//
// Ví dụ:
//
// readGPS(3000);
//
// ESP32 sẽ chờ tối đa khoảng 3 giây để tìm một vị trí GPS
// hợp lệ.
//
// Return:
//
// true
//      Có GPS fix hợp lệ.
//
// false
//      Chưa có GPS fix trong thời gian timeout.
//
// ============================================================

bool readGPS(uint32_t timeoutMs);


// ============================================================
// GPS FIX STATUS
// ============================================================
//
// Cho biết lần đọc GPS gần nhất có vị trí hợp lệ hay không.
//
// true  = latitude / longitude hợp lệ.
// false = chưa có fix.
//
// ============================================================

bool isGPSValid();


// ============================================================
// LATITUDE
// ============================================================
//
// Trả về latitude hiện tại.
//
// Ví dụ:
//
// 10.xxxxxx
//
// ============================================================

double getGPSLatitude();


// ============================================================
// LONGITUDE
// ============================================================
//
// Trả về longitude hiện tại.
//
// Ví dụ:
//
// 106.xxxxxx
//
// ============================================================

double getGPSLongitude();


// ============================================================
// SPEED
// ============================================================
//
// Trả về tốc độ do GPS cung cấp.
//
// Đơn vị:
//
// km/h
//
// Nếu chưa có speed hợp lệ,
// implementation trong gps.cpp sẽ trả giá trị phù hợp,
// thông thường là 0.
//
// ============================================================

double getGPSSpeedKmh();


// ============================================================
// SATELLITES
// ============================================================
//
// Trả về số vệ tinh GPS hiện tại.
//
// Ví dụ:
//
// 0  = chưa thấy vệ tinh
// 5  = đang thấy 5 vệ tinh
// 8  = đang thấy 8 vệ tinh
//
// ============================================================

uint32_t getGPSSatellites();


// ============================================================
// UART CHARACTER COUNT
// ============================================================
//
// Trả về tổng số ký tự NMEA TinyGPS++ đã xử lý.
//
// Hàm này rất hữu ích để debug.
//
// Ví dụ:
//
// charsProcessed == 0
//
// Có thể kiểm tra:
//
// - GPS chưa cấp nguồn
// - TX/RX sai
// - sai baud
// - UART chưa nhận dữ liệu
//
// Nếu charsProcessed > 0 nhưng GPS chưa fix:
//
// UART có thể vẫn hoạt động,
// nhưng GPS chưa có vị trí hợp lệ.
//
// ============================================================

uint32_t getGPSCharactersProcessed();


// ============================================================
// GPS DATA AGE
// ============================================================
//
// Trả về tuổi của location GPS hiện tại.
//
// Đơn vị:
//
// millisecond.
//
// Ví dụ:
//
// 500 ms
//      dữ liệu khá mới.
//
// 10000 ms
//      dữ liệu đã cũ.
//
// Nếu location chưa từng hợp lệ,
// TinyGPS++ có thể trả giá trị rất lớn.
//
// Hàm này giúp main.cpp tránh gửi tọa độ quá cũ lên Azure.
//
// ============================================================

uint32_t getGPSLocationAge();


// ============================================================
// GPS DATA READY FOR CLOUD
// ============================================================
//
// Kiểm tra dữ liệu GPS có đủ điều kiện để gửi Azure hay không.
//
// Dự kiến gps.cpp sẽ kiểm tra:
//
// 1. GPS location hợp lệ.
// 2. Latitude hợp lệ.
// 3. Longitude hợp lệ.
// 4. Location không quá cũ.
//
// Return:
//
// true
//      Có thể gửi POST /api/gps.
//
// false
//      Không nên gửi.
//
// ============================================================

bool isGPSReadyForUpload();


#endif // GPS_H