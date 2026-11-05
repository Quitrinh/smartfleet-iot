#include <Arduino.h>

#include "drivers/gsm.h"
#include "drivers/gps.h"
#include "drivers/rfid.h"
#include "drivers/keypad.h"
#include "drivers/lcd.h"
#include "drivers/camera.h"

#include "core/globals.h"


// ============================================================
// DEVICE
// ============================================================

static const char *DEVICE_ID = "TRUCK_001";


// ============================================================
// AUTH STATE
// ============================================================

enum AuthState
{
    WAIT_RFID,
    WAIT_PIN,
    SENDING_AZURE
};

static AuthState authState = WAIT_RFID;


// ============================================================
// AUTH DATA
// ============================================================

static String currentRFID = "";
static String currentPIN = "";


// ============================================================
// DRIVER / VEHICLE
// ============================================================

static String currentDriverCode = "";
static String currentDriverName = "";
static String currentVehicleCode = "";


// ============================================================
// CURRENT TRIP
// ============================================================

static bool tripActive = false;

static long currentTripId = 0;

static String currentTripCode = "";
static String currentTripStatus = "";
static String currentDestination = "";


// ============================================================
// GPS SETTINGS
// ============================================================

static const uint32_t GPS_UPLOAD_INTERVAL_MS = 15000;
static const uint32_t GPS_READ_TIMEOUT_MS = 2000;

static uint32_t lastGPSUploadTime = 0;
static const uint32_t CAMERA_UPLOAD_INTERVAL_MS = 10000;
static uint32_t lastCameraUploadTime = 0;



// ============================================================
// CAMERA STATE
// ============================================================

static bool cameraInitialized = false;


// ============================================================
// RESET AUTH
// ============================================================

static void resetAuthSession()
{
    currentRFID = "";
    currentPIN = "";

    authState = WAIT_RFID;

    Serial.println();
    Serial.println("================================");
    Serial.println(" WAITING FOR RFID");
    Serial.println("================================");

    lcdShowMessage(
        "Scan RFID",
        "Waiting..."
    );
}


// ============================================================
// RESET TRIP
// ============================================================

static void resetTripSession()
{
    tripActive = false;

    currentTripId = 0;

    currentTripCode = "";
    currentTripStatus = "";
    currentDestination = "";

    lastGPSUploadTime = 0;
    lastCameraUploadTime = 0;
}


// ============================================================
// PRINT CURRENT TRIP
// ============================================================

static void printCurrentTrip()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" CURRENT TRIP");
    Serial.println("================================");

    Serial.print("Trip ID     : ");
    Serial.println(currentTripId);

    Serial.print("Trip Code   : ");
    Serial.println(currentTripCode);

    Serial.print("Status      : ");
    Serial.println(currentTripStatus);

    Serial.print("Destination : ");
    Serial.println(currentDestination);

    Serial.println("================================");
}


// ============================================================
// ACTIVATE TRIP
// ============================================================

static void activateTrip(
    long tripId,
    const String &tripCode,
    const String &tripStatus,
    const String &destination
)
{
    currentTripId = tripId;
    currentTripCode = tripCode;
    currentTripStatus = tripStatus;
    currentDestination = destination;

    tripActive = true;

    lastGPSUploadTime = 0;

    Serial.println();
    Serial.println("================================");
    Serial.println(" TRIP ACTIVE");
    Serial.println("================================");

    Serial.print("Driver      : ");
    Serial.println(currentDriverCode);

    Serial.print("Vehicle     : ");
    Serial.println(currentVehicleCode);

    Serial.print("Trip        : ");
    Serial.println(currentTripCode);

    Serial.print("Status      : ");
    Serial.println(currentTripStatus);

    Serial.print("Destination : ");
    Serial.println(currentDestination);

    Serial.println("================================");

    lcdShowMessage(
        "Trip Active",
        currentTripCode.c_str()
    );

    // Flask camera endpoint is now implemented.
    // Upload immediately on the next loop iteration.
    lastCameraUploadTime = 0;
}


// ============================================================
// CAMERA TEST
// ============================================================

static void testCameraCapture()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" CAMERA CAPTURE TEST");
    Serial.println("================================");

    if (!cameraInitialized)
    {
        Serial.println(
            "[CAMERA] Camera is not initialized."
        );

        Serial.println("================================");

        return;
    }


    if (!isCameraReady())
    {
        Serial.println(
            "[CAMERA] Camera is not ready."
        );

        Serial.println("================================");

        return;
    }


    // --------------------------------------------------------
    // Capture JPEG
    // --------------------------------------------------------

    camera_fb_t *frame =
        captureCameraFrame();


    if (frame == nullptr)
    {
        Serial.println(
            "[CAMERA] Capture failed."
        );

        Serial.println("================================");

        return;
    }


    // --------------------------------------------------------
    // Print frame information
    // --------------------------------------------------------

    Serial.println(
        "[OK] JPEG captured"
    );


    Serial.print(
        "[CAMERA] Width  : "
    );

    Serial.println(
        frame->width
    );


    Serial.print(
        "[CAMERA] Height : "
    );

    Serial.println(
        frame->height
    );


    Serial.print(
        "[CAMERA] Size   : "
    );

    Serial.print(
        frame->len
    );

    Serial.println(
        " bytes"
    );


    // --------------------------------------------------------
    // IMPORTANT
    // --------------------------------------------------------
    //
    // Hiện tại CHƯA upload Azure.
    //
    // Bước tiếp theo gsm.cpp sẽ sử dụng:
    //
    // frame->buf
    // frame->len
    //
    // để upload JPEG qua A7680C.
    //
    // --------------------------------------------------------


    // --------------------------------------------------------
    // Release framebuffer
    // --------------------------------------------------------

    releaseCameraFrame(frame);


    Serial.println(
        "[OK] Camera test complete."
    );

    Serial.println("================================");
}




// ============================================================
// UPLOAD ONE CAMERA SNAPSHOT
// ============================================================
//
// This helper is intentionally NOT called automatically yet.
// Enable it only after Flask implements:
//
// POST /api/camera/upload
//
// ============================================================

static bool uploadCameraSnapshot()
{
    if (!cameraInitialized || !isCameraReady())
    {
        Serial.println(
            "[CAMERA CLOUD] Camera not ready."
        );

        return false;
    }

    camera_fb_t *frame =
        captureCameraFrame();

    if (frame == nullptr)
    {
        Serial.println(
            "[CAMERA CLOUD] Capture failed."
        );

        return false;
    }

    // The framebuffer remains valid during the synchronous
    // modem upload. It is always returned afterwards.
    CameraUploadResult uploadResult =
        postCameraImage(
            DEVICE_ID,
            currentTripId,
            frame->buf,
            frame->len
        );

    releaseCameraFrame(frame);

    if (!uploadResult.requestOK)
    {
        Serial.print(
            "[CAMERA CLOUD] Request failed: "
        );

        Serial.println(
            uploadResult.reason
        );

        return false;
    }

    if (!uploadResult.uploaded)
    {
        Serial.print(
            "[CAMERA CLOUD] Upload not confirmed: "
        );

        Serial.println(
            uploadResult.reason
        );

        return false;
    }

    Serial.println(
        "[CAMERA CLOUD] Snapshot uploaded."
    );

    return true;
}



// ============================================================
// PERIODIC CAMERA CLOUD UPLOAD
// ============================================================

static void handleCameraCloudUpload()
{
    if (!tripActive || currentTripId <= 0)
        return;

    if (!cameraInitialized || !isCameraReady())
        return;

    const uint32_t now = millis();

    if (
        lastCameraUploadTime != 0 &&
        (uint32_t)(now - lastCameraUploadTime) < CAMERA_UPLOAD_INTERVAL_MS
    )
        return;

    lastCameraUploadTime = now;

    Serial.println();
    Serial.println("================================");
    Serial.println(" CAMERA -> A7680C -> 4G -> AZURE");
    Serial.println("================================");

    if (!uploadCameraSnapshot())
    {
        Serial.println("[CAMERA CLOUD] Upload failed; retry next interval.");
    }
}

// ============================================================
// HANDLE AUTH GRANTED
// ============================================================

static void handleAuthGranted(
    const VehicleAuthResult &auth
)
{
    currentDriverCode =
        auth.driverCode;

    currentDriverName =
        auth.fullName;

    currentVehicleCode =
        auth.vehicleCode;


    Serial.println();
    Serial.println("================================");
    Serial.println(" AUTH GRANTED");
    Serial.println("================================");

    Serial.print("DRIVER : ");
    Serial.println(currentDriverCode);

    Serial.print("NAME   : ");
    Serial.println(currentDriverName);

    Serial.print("VEHICLE: ");
    Serial.println(currentVehicleCode);


    // ========================================================
    // NO TRIP ASSIGNED
    // ========================================================

    if (
        !auth.hasTrip ||
        auth.tripId <= 0
    )
    {
        Serial.println();

        Serial.println(
            "[TRIP] No assigned trip."
        );


        lcdShowMessage(
            "Access Granted",
            "No trip"
        );


        resetTripSession();

        delay(1500);

        resetAuthSession();

        return;
    }


    // ========================================================
    // COPY TRIP DATA
    // ========================================================

    currentTripId =
        auth.tripId;

    currentTripCode =
        auth.tripCode;

    currentTripStatus =
        auth.tripStatus;

    currentDestination =
        auth.destinationName;


    printCurrentTrip();


    // ========================================================
    // EXISTING ACTIVE TRIP
    // ========================================================

    if (
        currentTripStatus ==
        "IN_PROGRESS"
    )
    {
        Serial.println();

        Serial.println(
            "[TRIP] Existing active trip detected."
        );

        Serial.println(
            "[TRIP] Restoring trip session."
        );


        activateTrip(
            currentTripId,
            currentTripCode,
            currentTripStatus,
            currentDestination
        );


        currentPIN = "";

        authState = WAIT_RFID;

        return;
    }


    // ========================================================
    // START NEW TRIP
    // ========================================================

    if (
        currentTripStatus == "ASSIGNED" ||
        currentTripStatus == "READY"
    )
    {
        Serial.println();

        Serial.println(
            "[TRIP] Starting trip..."
        );


        lcdShowMessage(
            "Starting Trip",
            currentTripCode.c_str()
        );


        TripStartResult startResult =
            postStartTrip(
                DEVICE_ID,
                currentTripId
            );


        // ----------------------------------------------------
        // REQUEST FAILED
        // ----------------------------------------------------

        if (!startResult.requestOK)
        {
            Serial.println();

            Serial.println(
                "[TRIP ERROR] Request failed."
            );

            Serial.print(
                "[TRIP ERROR] Reason: "
            );

            Serial.println(
                startResult.reason
            );


            lcdShowMessage(
                "Trip Error",
                startResult.reason.c_str()
            );


            resetTripSession();

            delay(2000);

            resetAuthSession();

            return;
        }


        // ----------------------------------------------------
        // START REJECTED
        // ----------------------------------------------------

        if (
            !startResult.started &&
            !startResult.alreadyStarted
        )
        {
            Serial.println();

            Serial.println(
                "[TRIP ERROR] Start rejected."
            );

            Serial.print(
                "[TRIP ERROR] Reason: "
            );

            Serial.println(
                startResult.reason
            );


            lcdShowMessage(
                "Start Rejected",
                startResult.reason.c_str()
            );


            resetTripSession();

            delay(2000);

            resetAuthSession();

            return;
        }


        // ----------------------------------------------------
        // UPDATE TRIP INFORMATION
        // ----------------------------------------------------

        currentTripId =
            startResult.tripId;

        currentTripCode =
            startResult.tripCode;

        currentTripStatus =
            startResult.tripStatus;

        currentDestination =
            startResult.destinationName;


        if (
            startResult.driverCode.length() > 0
        )
        {
            currentDriverCode =
                startResult.driverCode;
        }


        if (
            startResult.fullName.length() > 0
        )
        {
            currentDriverName =
                startResult.fullName;
        }


        if (
            startResult.vehicleCode.length() > 0
        )
        {
            currentVehicleCode =
                startResult.vehicleCode;
        }


        activateTrip(
            currentTripId,
            currentTripCode,
            currentTripStatus,
            currentDestination
        );


        currentPIN = "";

        authState = WAIT_RFID;

        return;
    }


    // ========================================================
    // INVALID TRIP STATUS
    // ========================================================

    Serial.println();

    Serial.print(
        "[TRIP] Cannot start trip with status: "
    );

    Serial.println(
        currentTripStatus
    );


    lcdShowMessage(
        "Trip Status",
        currentTripStatus.c_str()
    );


    resetTripSession();

    delay(2000);

    resetAuthSession();
}


// ============================================================
// HANDLE RFID
// ============================================================

static void handleRFID()
{
    String uid = "";


    if (!readRFID(uid))
    {
        return;
    }


    if (uid.length() == 0)
    {
        return;
    }


    currentRFID = uid;

    currentPIN = "";


    Serial.println();
    Serial.println("================================");
    Serial.println(" RFID DETECTED");
    Serial.println("================================");

    Serial.print("UID: ");
    Serial.println(currentRFID);


    lcdShowMessage(
        "RFID OK",
        "Enter PIN"
    );


    authState = WAIT_PIN;
}


// ============================================================
// HANDLE PIN
// ============================================================

static void handlePIN()
{
    char key =
        readKeypad();


    if (key == '\0')
    {
        return;
    }


    // ========================================================
    // CANCEL
    // ========================================================

    if (key == 'C')
    {
        Serial.println();

        Serial.println(
            "[AUTH] Cancelled."
        );


        currentPIN = "";

        resetAuthSession();

        return;
    }


    // ========================================================
    // DELETE
    // ========================================================

    if (key == '*')
    {
        if (
            currentPIN.length() > 0
        )
        {
            currentPIN.remove(
                currentPIN.length() - 1
            );
        }


        Serial.print(
            "[PIN] Length: "
        );

        Serial.println(
            currentPIN.length()
        );


        String masked = "";


        for (
            size_t i = 0;
            i < currentPIN.length();
            i++
        )
        {
            masked += "*";
        }


        lcdShowMessage(
            "Enter PIN",
            masked.c_str()
        );


        return;
    }


    // ========================================================
    // SUBMIT
    // ========================================================

    if (key == '#')
    {
        if (
            currentPIN.length() == 0
        )
        {
            Serial.println(
                "[PIN] PIN is empty."
            );

            return;
        }


        authState =
            SENDING_AZURE;


        Serial.println();

        Serial.println(
            "[AUTH] Sending authentication..."
        );


        lcdShowMessage(
            "Checking...",
            "Please wait"
        );


        VehicleAuthResult auth =
            postVehicleAuth(
                DEVICE_ID,
                currentRFID,
                currentPIN
            );


        // ----------------------------------------------------
        // SECURITY:
        // Clear PIN immediately after request
        // ----------------------------------------------------

        currentPIN = "";


        // ----------------------------------------------------
        // NETWORK ERROR
        // ----------------------------------------------------

        if (!auth.requestOK)
        {
            Serial.println();

            Serial.println("================================");
            Serial.println(" AUTH REQUEST FAILED");
            Serial.println("================================");

            Serial.print("Reason: ");
            Serial.println(auth.reason);


            lcdShowMessage(
                "Network Error",
                auth.reason.c_str()
            );


            delay(2000);

            resetAuthSession();

            return;
        }


        // ----------------------------------------------------
        // ACCESS DENIED
        // ----------------------------------------------------

        if (!auth.granted)
        {
            Serial.println();

            Serial.println("================================");
            Serial.println(" ACCESS DENIED");
            Serial.println("================================");

            Serial.print("Reason: ");
            Serial.println(auth.reason);


            lcdShowMessage(
                "Access Denied",
                auth.reason.c_str()
            );


            delay(2000);

            resetAuthSession();

            return;
        }


        // ----------------------------------------------------
        // ACCESS GRANTED
        // ----------------------------------------------------

        handleAuthGranted(auth);

        return;
    }


    // ========================================================
    // DIGIT INPUT
    // ========================================================

    if (
        key >= '0' &&
        key <= '9'
    )
    {
        if (
            currentPIN.length() >= 8
        )
        {
            return;
        }


        currentPIN += key;


        // Never print PIN itself.

        Serial.print(
            "[PIN] Length: "
        );

        Serial.println(
            currentPIN.length()
        );


        String masked = "";


        for (
            size_t i = 0;
            i < currentPIN.length();
            i++
        )
        {
            masked += "*";
        }


        lcdShowMessage(
            "Enter PIN",
            masked.c_str()
        );
    }
}


// ============================================================
// GPS TRACKING
// ============================================================

static void handleGPSTracking()
{
    if (!tripActive)
    {
        return;
    }


    if (currentTripId <= 0)
    {
        return;
    }


    const uint32_t now =
        millis();


    if (
        lastGPSUploadTime != 0 &&
        (uint32_t)(
            now - lastGPSUploadTime
        ) < GPS_UPLOAD_INTERVAL_MS
    )
    {
        return;
    }


    // Avoid starting another upload immediately
    // if the request is slow.

    lastGPSUploadTime = now;


    Serial.println();
    Serial.println("================================");
    Serial.println(" GPS TRACKING");
    Serial.println("================================");

    Serial.print("Trip ID: ");
    Serial.println(currentTripId);


    bool gpsOK =
        readGPS(
            GPS_READ_TIMEOUT_MS
        );


    if (!gpsOK)
    {
        Serial.println(
            "[GPS] No valid location."
        );

        Serial.println(
            "[GPS] Skip cloud upload."
        );

        return;
    }


    if (!isGPSReadyForUpload())
    {
        Serial.println(
            "[GPS] Location not ready for upload."
        );

        return;
    }


    const double latitude =
        getGPSLatitude();


    const double longitude =
        getGPSLongitude();


    const double speedKmh =
        getGPSSpeedKmh();


    const uint32_t satellites =
        getGPSSatellites();


    Serial.println(
        "[GPS] Sending location to Azure..."
    );


    GPSUploadResult gpsResult =
        postGPSLocation(
            DEVICE_ID,
            currentTripId,
            latitude,
            longitude,
            speedKmh,
            satellites
        );


    // ========================================================
    // REQUEST FAILED
    // ========================================================

    if (!gpsResult.requestOK)
    {
        Serial.println();

        Serial.println(
            "[GPS CLOUD] Request failed."
        );

        Serial.print(
            "[GPS CLOUD] Reason: "
        );

        Serial.println(
            gpsResult.reason
        );

        return;
    }


    // ========================================================
    // NOT SAVED
    // ========================================================

    if (!gpsResult.saved)
    {
        Serial.println();

        Serial.println(
            "[GPS CLOUD] GPS was not saved."
        );

        Serial.print(
            "[GPS CLOUD] Reason: "
        );

        Serial.println(
            gpsResult.reason
        );

        return;
    }


    // ========================================================
    // SUCCESS
    // ========================================================

    Serial.println();

    Serial.println("================================");
    Serial.println(" GPS SAVED TO CLOUD");
    Serial.println("================================");

    Serial.print("GPS LOG ID : ");
    Serial.println(gpsResult.gpsLogId);

    Serial.print("Trip ID    : ");
    Serial.println(currentTripId);

    Serial.println("================================");
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // ========================================================
    // SERIAL
    // ========================================================

    Serial.begin(115200);

    delay(1500);


    Serial.println();
    Serial.println();

    Serial.println("================================");
    Serial.println(" SMARTFLEET IOT");
    Serial.println(" ESP32-S3");
    Serial.println("================================");


    // ========================================================
    // LCD
    // ========================================================

    Serial.println();
    Serial.println("[INIT] LCD...");


    if (!initLCD())
    {
        Serial.println(
            "[WARNING] LCD initialization failed."
        );
    }


    lcdShowMessage(
        "SmartFleet",
        "Starting..."
    );


    // ========================================================
    // KEYPAD
    // ========================================================

    Serial.println();
    Serial.println("[INIT] Keypad...");


    if (!initKeypad())
    {
        Serial.println(
            "[WARNING] Keypad initialization failed."
        );
    }


    // ========================================================
    // RFID
    // ========================================================

    Serial.println();
    Serial.println("[INIT] RFID...");


    if (!initRFID())
    {
        Serial.println(
            "[WARNING] RFID initialization failed."
        );
    }


    // ========================================================
    // CAMERA OV3660
    // ========================================================

    Serial.println();
    Serial.println("================================");
    Serial.println(" CAMERA INITIALIZATION");
    Serial.println("================================");


    lcdShowMessage(
        "Camera",
        "Starting..."
    );


    cameraInitialized =
        initCamera();


    if (!cameraInitialized)
    {
        Serial.println();

        Serial.println(
            "[WARNING] Camera initialization failed."
        );


        lcdShowMessage(
            "Camera Error",
            "Continue..."
        );


        // Camera failure must NOT stop:
        //
        // RFID
        // GPS
        // GSM
        // 4G
        //
        // SmartFleet continues in degraded mode.

        delay(1000);
    }
    else
    {
        Serial.println();

        Serial.println(
            "[OK] Camera initialized."
        );


        lcdShowMessage(
            "Camera Ready",
            "Testing..."
        );


        // ----------------------------------------------------
        // Test one JPEG capture during boot
        // ----------------------------------------------------

        testCameraCapture();


        delay(500);
    }


    // ========================================================
    // GPS
    // ========================================================

    Serial.println();
    Serial.println("[INIT] GPS...");


    if (!initGPS())
    {
        Serial.println(
            "[WARNING] GPS initialization failed."
        );
    }


    // ========================================================
    // GSM / A7680C
    // ========================================================

    Serial.println();
    Serial.println(
        "[INIT] GSM / A7680C..."
    );


    lcdShowMessage(
        "4G Modem",
        "Starting..."
    );


    if (!initGSM())
    {
        Serial.println(
            "[ERROR] GSM initialization failed."
        );


        lcdShowMessage(
            "GSM Error",
            "Check modem"
        );
    }
    else
    {
        Serial.println(
            "[OK] GSM initialized."
        );
    }


    // ========================================================
    // CONNECT 4G
    // ========================================================

    Serial.println();
    Serial.println(
        "[INIT] Connecting 4G..."
    );


    if (connect4G())
    {
        Serial.println();

        Serial.println("================================");
        Serial.println(" 4G DATA READY");
        Serial.println("================================");


        lcdShowMessage(
            "4G Ready",
            "Scan RFID"
        );
    }
    else
    {
        Serial.println();

        Serial.println(
            "[ERROR] 4G connection failed."
        );


        lcdShowMessage(
            "4G Error",
            "Check network"
        );
    }


    // ========================================================
    // RESET SYSTEM STATE
    // ========================================================

    resetTripSession();

    resetAuthSession();


    // ========================================================
    // SYSTEM STATUS
    // ========================================================

    Serial.println();
    Serial.println("================================");
    Serial.println(" SMARTFLEET SYSTEM READY");
    Serial.println("================================");

    Serial.print("Device : ");
    Serial.println(DEVICE_ID);

    Serial.print("Camera : ");

    if (cameraInitialized)
    {
        Serial.println("READY");
    }
    else
    {
        Serial.println("OFFLINE");
    }

    Serial.println("GPS    : INITIALIZED");
    Serial.println("4G     : INITIALIZED");

    Serial.println("================================");
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // ========================================================
    // 1. GPS CLOUD TRACKING
    // ========================================================
    //
    // GPS is uploaded only while a trip is active.
    //
    // ========================================================

    handleGPSTracking();

    handleCameraCloudUpload();


    // ========================================================
    // 2. AUTHENTICATION STATE MACHINE
    // ========================================================

    switch (authState)
    {
        // ----------------------------------------------------
        // WAIT RFID
        // ----------------------------------------------------

        case WAIT_RFID:
        {
            if (!tripActive)
            {
                handleRFID();
            }

            break;
        }


        // ----------------------------------------------------
        // WAIT PIN
        // ----------------------------------------------------

        case WAIT_PIN:
        {
            handlePIN();

            break;
        }


        // ----------------------------------------------------
        // SENDING TO AZURE
        // ----------------------------------------------------

        case SENDING_AZURE:
        {
            // Request is handled synchronously
            // inside handlePIN().

            break;
        }
    }


    // ========================================================
    // 3. FREE CPU
    // ========================================================

    vTaskDelay(
        pdMS_TO_TICKS(10)
    );
}