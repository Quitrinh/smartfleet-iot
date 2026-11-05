#ifndef GSM_H
#define GSM_H

#include <Arduino.h>


// ============================================================
// VEHICLE AUTH RESULT
// ============================================================

struct VehicleAuthResult
{
    // Request HTTP/API thành công
    bool requestOK;

    // Backend cho phép truy cập xe
    bool granted;

    // Lý do
    String reason;


    // ========================================================
    // DRIVER
    // ========================================================

    String driverCode;
    String fullName;


    // ========================================================
    // VEHICLE
    // ========================================================

    String vehicleCode;


    // ========================================================
    // TRIP
    // ========================================================

    bool hasTrip;

    long tripId;

    String tripCode;

    String tripStatus;

    String destinationName;
};


// ============================================================
// TRIP START RESULT
// ============================================================
//
// Response từ:
//
// POST /api/trips/start
//
// ============================================================

struct TripStartResult
{
    // HTTP/API request thành công
    bool requestOK;

    // Trip đã được start
    bool started;

    // Trip đã chạy từ trước
    bool alreadyStarted;

    // Lý do
    String reason;


    // ========================================================
    // TRIP
    // ========================================================

    long tripId;

    String tripCode;

    String tripStatus;

    String originName;

    String destinationName;

    // Thời điểm trip bắt đầu
    String startedAt;


    // ========================================================
    // DRIVER
    // ========================================================

    String driverCode;

    String fullName;


    // ========================================================
    // VEHICLE
    // ========================================================

    String vehicleCode;

    // Trạng thái vehicle sau khi start trip
    // Ví dụ:
    // IN_TRIP
    String vehicleStatus;
};


// ============================================================
// GPS UPLOAD RESULT
// ============================================================
//
// Response từ:
//
// POST /api/gps
//
// ============================================================

struct GPSUploadResult
{
    // HTTP request thành công
    bool requestOK;

    // GPS đã được lưu PostgreSQL
    bool saved;

    // Lý do
    String reason;

    // ID của row gps_logs vừa tạo
    long gpsLogId;
};


// ============================================================
// GSM INITIALIZATION
// ============================================================

bool initGSM();


// ============================================================
// GSM TEST
// ============================================================

bool testGSM();


// ============================================================
// SEND AT COMMAND
// ============================================================

bool sendATCommand(
    const char *command,
    const char *expected,
    uint32_t timeoutMs
);


// ============================================================
// CONNECT 4G
// ============================================================

bool connect4G();


// ============================================================
// VEHICLE AUTHENTICATION
// ============================================================
//
// POST:
//
// /api/auth/vehicle
//
// ESP32 gửi:
//
// {
//     "device_id": "...",
//     "rfid_uid": "...",
//     "pin": "..."
// }
//
// ============================================================

VehicleAuthResult postVehicleAuth(
    const String &deviceId,
    const String &rfidUid,
    const String &pin
);


// ============================================================
// START TRIP
// ============================================================
//
// POST:
//
// /api/trips/start
//
// {
//     "device_id": "TRUCK_001",
//     "trip_id": 1
// }
//
// ============================================================

TripStartResult postStartTrip(
    const String &deviceId,
    long tripId
);


// ============================================================
// GPS LOCATION UPLOAD
// ============================================================
//
// POST:
//
// /api/gps
//
// {
//     "device_id": "TRUCK_001",
//     "trip_id": 1,
//     "latitude": ...,
//     "longitude": ...,
//     "speed_kmh": ...,
//     "satellites": ...
// }
//
// ============================================================

GPSUploadResult postGPSLocation(
    const String &deviceId,
    long tripId,
    double latitude,
    double longitude,
    double speedKmh,
    uint32_t satellites
);


// ============================================================
// ACCESS EVENT
// ============================================================
//
// Giữ lại để tương thích code cũ.
//
// Authentication endpoint đã tự ghi access log,
// vì vậy không cần gọi lại sau postVehicleAuth().
//
// ============================================================

bool postAccessEvent(
    const String &deviceId,
    const String &event,
    const String &userId,
    const String &rfidUid,
    double latitude,
    double longitude
);


// ============================================================
// SMS
// ============================================================

bool sendSMS(
    const char *phone,
    const String &message
);


// ============================================================
// CREATE GPS MESSAGE
// ============================================================

String createGPSMessage();


// ============================================================
// SEND GPS SMS
// ============================================================

bool sendGPSSMS(
    const char *phone
);


// ============================================================
// LEGACY GPS SMS
// ============================================================

bool sendGPSToAllPhones();



// ============================================================
// CAMERA UPLOAD RESULT
// ============================================================
//
// Planned backend:
// POST /api/camera/upload?device_id=...&trip_id=...
//
// Body:
// raw JPEG bytes
//
// Content-Type:
// image/jpeg
//
// ============================================================

struct CameraUploadResult
{
    bool requestOK;
    bool uploaded;
    int httpStatus;
    String reason;
};


// ============================================================
// CAMERA JPEG UPLOAD
// ============================================================

CameraUploadResult postCameraImage(
    const String &deviceId,
    long tripId,
    const uint8_t *jpegData,
    size_t jpegLength
);


#endif // GSM_H