#include <Arduino.h>

#include "drivers/gsm.h"
#include "config/pins.h"
#include "core/globals.h"

// ============================================================
// A7680C UART
// ============================================================

static HardwareSerial gsmSerial(2);

#define GSM_BAUD 115200

// ============================================================
// AZURE
// ============================================================

static const char *AZURE_BASE_URL =
    "https://smartfleet-api-dev-hdffarevhgdzbzbv.eastasia-01.azurewebsites.net";

static const char *AUTH_URL =
    "https://smartfleet-api-dev-hdffarevhgdzbzbv.eastasia-01.azurewebsites.net/api/auth/vehicle";

static const char *START_TRIP_URL =
    "https://smartfleet-api-dev-hdffarevhgdzbzbv.eastasia-01.azurewebsites.net/api/trips/start";

static const char *GPS_URL =
    "https://smartfleet-api-dev-hdffarevhgdzbzbv.eastasia-01.azurewebsites.net/api/gps";

static const char *ACCESS_URL =
    "https://smartfleet-api-dev-hdffarevhgdzbzbv.eastasia-01.azurewebsites.net/api/access";

static const char *CAMERA_URL =
    "https://smartfleet-api-dev-hdffarevhgdzbzbv.eastasia-01.azurewebsites.net/api/camera/upload";

// ============================================================
// SMS
// ============================================================
//
// Thay bằng số điện thoại của bạn.
// Không cần thay đổi phần code SMS bên dưới.
//
// ============================================================

static const char *smsPhones[] =
{
    "+84xxxxxxxxx",
    "+84xxxxxxxxx"
};

static const int smsPhoneCount =
    sizeof(smsPhones) / sizeof(smsPhones[0]);

// ============================================================
// HTTP RESULT INTERNAL
// ============================================================

struct HTTPResult
{
    bool commandOK;
    bool actionReceived;

    int status;
    int dataLength;

    String actionResponse;
    String body;
    String error;
};

// ============================================================
// CLEAR GSM BUFFER
// ============================================================

static void clearGSMBuffer()
{
    while (gsmSerial.available() > 0)
    {
        gsmSerial.read();
    }
}

// ============================================================
// DRAIN GSM INPUT
// ============================================================

static void drainGSMInput(uint32_t quietMs = 200)
{
    uint32_t lastData = millis();

    while (millis() - lastData < quietMs)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            Serial.write(c);

            lastData = millis();
        }

        delay(5);
    }
}

// ============================================================
// SEND AT COMMAND
// ============================================================

bool sendATCommand(
    const char *command,
    const char *expected,
    uint32_t timeoutMs)
{
    clearGSMBuffer();

    Serial.println();
    Serial.print("[GSM TX] ");
    Serial.println(command);

    gsmSerial.println(command);

    String response;

    uint32_t start = millis();

    while (millis() - start < timeoutMs)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            if (c == '\0')
                continue;

            Serial.write(c);

            response += c;

            if (
                expected != nullptr &&
                strlen(expected) > 0 &&
                response.indexOf(expected) >= 0)
            {
                Serial.println();
                Serial.print("[GSM] FOUND: ");
                Serial.println(expected);

                return true;
            }

            if (response.indexOf("ERROR") >= 0)
            {
                Serial.println();
                Serial.println("[GSM] ERROR");

                return false;
            }
        }

        delay(10);
    }

    // expected rỗng nghĩa là command được dùng best-effort.
    if (
        expected == nullptr ||
        strlen(expected) == 0)
    {
        return true;
    }

    Serial.println();
    Serial.println("[GSM] TIMEOUT");

    return false;
}

// ============================================================
// MODEM RESPONSIVE
// ============================================================

static bool ensureModemResponsive()
{
    for (int i = 1; i <= 3; i++)
    {
        Serial.print("[GSM] Modem check ");
        Serial.print(i);
        Serial.println("/3");

        if (
            sendATCommand(
                "AT",
                "OK",
                2000))
        {
            return true;
        }

        delay(500);
    }

    return false;
}

// ============================================================
// INIT GSM
// ============================================================

bool initGSM()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" GSM INITIALIZATION");
    Serial.println("================================");

    gsmSerial.begin(
        GSM_BAUD,
        SERIAL_8N1,
        GSM_RX_PIN,
        GSM_TX_PIN);

    delay(3000);

    for (int i = 1; i <= 10; i++)
    {
        Serial.print("[GSM] AT attempt: ");
        Serial.println(i);

        if (
            sendATCommand(
                "AT",
                "OK",
                2000))
        {
            Serial.println("[GSM] MODEM READY");

            sendATCommand(
                "ATE0",
                "OK",
                2000);

            sendATCommand(
                "AT+CMGF=1",
                "OK",
                2000);

            sendATCommand(
                "AT+CSCS=\"GSM\"",
                "OK",
                2000);

            // --------------------------------------------
            // DIAGNOSTIC
            // --------------------------------------------

            Serial.println();
            Serial.println("================================");
            Serial.println(" A7680C DIAGNOSTIC");
            Serial.println("================================");

            sendATCommand(
                "ATI",
                "OK",
                5000);

            sendATCommand(
                "AT+CGMR",
                "OK",
                5000);

            sendATCommand(
                "AT+CSSLCFG?",
                "OK",
                5000);

            Serial.println("================================");

            return true;
        }

        delay(1000);
    }

    Serial.println("[GSM] MODEM NOT RESPONDING");

    return false;
}

// ============================================================
// TEST GSM
// ============================================================

bool testGSM()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" A7680C STATUS TEST");
    Serial.println("================================");

    if (
        !sendATCommand(
            "AT+CPIN?",
            "READY",
            3000))
    {
        Serial.println("[GSM] SIM NOT READY");
        return false;
    }

    if (
        !sendATCommand(
            "AT+CSQ",
            "+CSQ:",
            3000))
    {
        Serial.println("[GSM] CSQ FAILED");
        return false;
    }

    if (
        !sendATCommand(
            "AT+CEREG?",
            "+CEREG:",
            3000))
    {
        Serial.println("[GSM] CEREG FAILED");
        return false;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println(" GSM TEST PASS");
    Serial.println("================================");

    return true;
}

// ============================================================
// CONNECT 4G
// ============================================================

bool connect4G()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" CONNECT 4G DATA");
    Serial.println("================================");

    if (
        !sendATCommand(
            "AT+CPIN?",
            "READY",
            5000))
    {
        Serial.println("[4G ERROR] SIM NOT READY");
        return false;
    }

    // --------------------------------------------------------
    // PACKET ATTACH
    // --------------------------------------------------------

    if (
        !sendATCommand(
            "AT+CGATT=1",
            "OK",
            10000))
    {
        Serial.println("[4G ERROR] CGATT FAILED");
        return false;
    }

    // --------------------------------------------------------
    // OPEN NETWORK
    // --------------------------------------------------------

    clearGSMBuffer();

    Serial.println();
    Serial.println("[GSM TX] AT+NETOPEN");

    gsmSerial.println("AT+NETOPEN");

    String response;

    uint32_t start = millis();

    while (millis() - start < 15000)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            Serial.write(c);
            response += c;
        }

        if (
            response.indexOf("+NETOPEN: 0") >= 0 ||
            response.indexOf("OK") >= 0)
        {
            break;
        }

        if (
            response.indexOf("Network is already opened") >= 0)
        {
            break;
        }

        delay(10);
    }

    // --------------------------------------------------------
    // VERIFY NETWORK
    // --------------------------------------------------------

    if (
        !sendATCommand(
            "AT+NETOPEN?",
            "+NETOPEN: 1",
            5000))
    {
        Serial.println("[4G ERROR] NETWORK NOT OPEN");
        return false;
    }

    Serial.println("[4G] NETWORK OPEN");

    // --------------------------------------------------------
    // IP ADDRESS
    // --------------------------------------------------------

    if (
        !sendATCommand(
            "AT+IPADDR",
            "+IPADDR:",
            5000))
    {
        Serial.println("[4G ERROR] NO IP ADDRESS");
        return false;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println(" 4G DATA READY");
    Serial.println("================================");

    return true;
}

// ============================================================
// PREPARE HTTP SESSION
// ============================================================

static bool prepareHTTPSession()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" PREPARE HTTP SESSION");
    Serial.println("================================");

    if (!ensureModemResponsive())
    {
        Serial.println("[HTTP ERROR] MODEM NOT READY");
        return false;
    }

    // Terminate previous session.
    sendATCommand(
        "AT+HTTPTERM",
        "",
        2000);

    delay(300);

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        Serial.print("[HTTP] HTTPINIT attempt ");
        Serial.print(attempt);
        Serial.println("/3");

        if (
            sendATCommand(
                "AT+HTTPINIT",
                "OK",
                5000))
        {
            Serial.println("[HTTP] SESSION READY");
            return true;
        }

        sendATCommand(
            "AT+HTTPTERM",
            "",
            2000);

        delay(500);
    }

    Serial.println("[HTTP ERROR] HTTPINIT FAILED");

    return false;
}

// ============================================================
// CONFIGURE TLS
// ============================================================

static bool configureAzureHTTPS()
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" CONFIGURE A7680C TLS");
    Serial.println("================================");

    // Context 0.
    //
    // sslversion = 4:
    // allow modem supported TLS selection.
    //
    // authmode = 0:
    // diagnostic mode without CA verification.
    //
    // Sau khi HTTPS hoạt động ổn định có thể cấu hình CA
    // verification riêng.

    if (
        !sendATCommand(
            "AT+CSSLCFG=\"sslversion\",0,4",
            "OK",
            5000))
    {
        Serial.println("[TLS ERROR] SSL VERSION");
        return false;
    }

    if (
        !sendATCommand(
            "AT+CSSLCFG=\"authmode\",0,0",
            "OK",
            5000))
    {
        Serial.println("[TLS ERROR] AUTH MODE");
        return false;
    }

    if (
        !sendATCommand(
            "AT+CSSLCFG=\"enableSNI\",0,1",
            "OK",
            5000))
    {
        Serial.println("[TLS ERROR] SNI");
        return false;
    }

    // Print actual configuration.
    sendATCommand(
        "AT+CSSLCFG?",
        "OK",
        5000);

    // Attach SSL context 0 to HTTP.
    if (
        !sendATCommand(
            "AT+HTTPPARA=\"SSLCFG\",0",
            "OK",
            5000))
    {
        Serial.println("[HTTPS ERROR] SSLCFG");
        return false;
    }

    Serial.println("[HTTPS] SSL context 0 selected.");
    Serial.println("================================");

    return true;
}

// ============================================================
// PARSE HTTPACTION
// ============================================================

static bool parseHTTPAction(
    const String &response,
    int &status,
    int &dataLength)
{
    int pos =
        response.indexOf("+HTTPACTION:");

    if (pos < 0)
        return false;

    int comma1 =
        response.indexOf(',', pos);

    if (comma1 < 0)
        return false;

    int comma2 =
        response.indexOf(',', comma1 + 1);

    if (comma2 < 0)
        return false;

    int end =
        response.indexOf('\n', comma2 + 1);

    if (end < 0)
        end = response.length();

    String statusText =
        response.substring(
            comma1 + 1,
            comma2);

    String lengthText =
        response.substring(
            comma2 + 1,
            end);

    statusText.trim();
    lengthText.trim();

    status = statusText.toInt();
    dataLength = lengthText.toInt();

    return true;
}

// ============================================================
// WAIT HTTP ACTION
// ============================================================

static HTTPResult waitHTTPAction(
    uint32_t timeoutMs)
{
    HTTPResult result;

    result.commandOK = false;
    result.actionReceived = false;
    result.status = 0;
    result.dataLength = 0;
    result.actionResponse = "";
    result.body = "";
    result.error = "";

    String response;

    uint32_t start = millis();

    while (millis() - start < timeoutMs)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            Serial.write(c);

            response += c;

            if (
                response.indexOf("\r\nOK\r\n") >= 0)
            {
                result.commandOK = true;
            }

            if (
                response.indexOf("*ATREADY") >= 0)
            {
                result.error =
                    "MODEM_RESTART_DURING_HTTPS";

                result.actionResponse = response;

                return result;
            }

            if (
                response.indexOf("+HTTPACTION:") >= 0)
            {
                int status = 0;
                int length = 0;

                // Wait until full line is received.
                delay(50);

                while (gsmSerial.available() > 0)
                {
                    char extra = gsmSerial.read();

                    Serial.write(extra);
                    response += extra;
                }

                if (
                    parseHTTPAction(
                        response,
                        status,
                        length))
                {
                    result.actionReceived = true;
                    result.status = status;
                    result.dataLength = length;
                    result.actionResponse = response;

                    return result;
                }
            }

            if (
                response.indexOf("ERROR") >= 0)
            {
                result.error =
                    "HTTPACTION_COMMAND_ERROR";

                result.actionResponse = response;

                return result;
            }
        }

        delay(10);
    }

    result.error = "HTTPACTION_TIMEOUT";
    result.actionResponse = response;

    return result;
}

// ============================================================
// HTTP READ
// ============================================================

static String readHTTPBody(
    int expectedLength)
{
    clearGSMBuffer();

    Serial.println();
    Serial.println("[HTTP] Reading response body...");

    if (expectedLength > 0)
    {
        gsmSerial.print("AT+HTTPREAD=0,");
        gsmSerial.println(expectedLength);
    }
    else
    {
        gsmSerial.println("AT+HTTPREAD=0,2048");
    }

    String response;

    uint32_t start = millis();

    while (millis() - start < 10000)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            Serial.write(c);
            response += c;

            if (
                response.indexOf("\r\nOK\r\n") >= 0)
            {
                delay(50);

                while (gsmSerial.available() > 0)
                {
                    char extra = gsmSerial.read();

                    Serial.write(extra);
                    response += extra;
                }

                return response;
            }

            if (
                response.indexOf("\r\nERROR\r\n") >= 0)
            {
                return response;
            }
        }

        delay(10);
    }

    return response;
}

// ============================================================
// HTTP POST JSON
// ============================================================

static HTTPResult httpPostJSON(
    const String &url,
    const String &json)
{
    HTTPResult result;

    result.commandOK = false;
    result.actionReceived = false;
    result.status = 0;
    result.dataLength = 0;
    result.actionResponse = "";
    result.body = "";
    result.error = "";

    if (!prepareHTTPSession())
    {
        result.error = "HTTP_SESSION_FAILED";
        return result;
    }

    if (!configureAzureHTTPS())
    {
        result.error = "TLS_CONFIG_FAILED";

        sendATCommand(
            "AT+HTTPTERM",
            "",
            2000);

        return result;
    }

    // --------------------------------------------------------
    // URL
    // --------------------------------------------------------

    String urlCommand =
        "AT+HTTPPARA=\"URL\",\"" +
        url +
        "\"";

    if (
        !sendATCommand(
            urlCommand.c_str(),
            "OK",
            5000))
    {
        result.error = "HTTP_URL_FAILED";

        sendATCommand(
            "AT+HTTPTERM",
            "",
            2000);

        return result;
    }

    // --------------------------------------------------------
    // CONTENT TYPE
    // --------------------------------------------------------

    if (
        !sendATCommand(
            "AT+HTTPPARA=\"CONTENT\",\"application/json\"",
            "OK",
            5000))
    {
        result.error = "HTTP_CONTENT_FAILED";

        sendATCommand(
            "AT+HTTPTERM",
            "",
            2000);

        return result;
    }

    // --------------------------------------------------------
    // HTTP DATA
    // --------------------------------------------------------

    String dataCommand =
        "AT+HTTPDATA=" +
        String(json.length()) +
        ",10000";

    if (
        !sendATCommand(
            dataCommand.c_str(),
            "DOWNLOAD",
            5000))
    {
        result.error = "HTTPDATA_FAILED";

        sendATCommand(
            "AT+HTTPTERM",
            "",
            2000);

        return result;
    }

    Serial.println();
    Serial.println("[HTTP] Sending JSON...");

    gsmSerial.print(json);

    // Wait for data OK.
    String dataResponse;

    uint32_t dataStart = millis();

    while (millis() - dataStart < 12000)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            Serial.write(c);
            dataResponse += c;

            if (
                dataResponse.indexOf("\r\nOK\r\n") >= 0)
            {
                goto data_sent;
            }

            if (
                dataResponse.indexOf("ERROR") >= 0)
            {
                result.error = "HTTPDATA_SEND_ERROR";

                sendATCommand(
                    "AT+HTTPTERM",
                    "",
                    2000);

                return result;
            }
        }

        delay(10);
    }

    result.error = "HTTPDATA_SEND_TIMEOUT";

    sendATCommand(
        "AT+HTTPTERM",
        "",
        2000);

    return result;

data_sent:

    // --------------------------------------------------------
    // POST
    // --------------------------------------------------------

    clearGSMBuffer();

    Serial.println();
    Serial.println("[GSM TX] AT+HTTPACTION=1");

    gsmSerial.println(
        "AT+HTTPACTION=1");

    result =
        waitHTTPAction(120000);

    Serial.println();
    Serial.println();
    Serial.println("[HTTP] HTTPACTION RESULT");

    Serial.print("[HTTP] STATUS : ");
    Serial.println(result.status);

    Serial.print("[HTTP] LENGTH : ");
    Serial.println(result.dataLength);

    if (!result.actionReceived)
    {
        if (result.error.length() == 0)
        {
            result.error =
                "NO_HTTPACTION_RESPONSE";
        }

        sendATCommand(
            "AT+HTTPTERM",
            "",
            3000);

        return result;
    }

    // --------------------------------------------------------
    // A76XX MODEM ERROR
    // --------------------------------------------------------

    if (
        result.status >= 700 &&
        result.status <= 799)
    {
        Serial.println();
        Serial.println("================================");
        Serial.println(" A7680C HTTP/TLS ERROR");
        Serial.println("================================");

        Serial.print("CODE: ");
        Serial.println(result.status);

        if (result.status == 715)
        {
            Serial.println(
                "[TLS ERROR] HTTPS handshake failed.");
        }

        result.error =
            "MODEM_HTTP_ERROR_" +
            String(result.status);

        sendATCommand(
            "AT+HTTPTERM",
            "",
            3000);

        return result;
    }

    // --------------------------------------------------------
    // READ BODY
    // --------------------------------------------------------

    if (result.dataLength > 0)
    {
        result.body =
            readHTTPBody(
                result.dataLength);
    }

    sendATCommand(
        "AT+HTTPTERM",
        "",
        3000);

    return result;
}

// ============================================================
// SIMPLE JSON HELPERS
// ============================================================

static String jsonObject(
    const String &json,
    const String &key)
{
    String pattern =
        "\"" + key + "\"";

    int p = json.indexOf(pattern);
    if (p < 0)
        return "";

    p = json.indexOf(':', p + pattern.length());
    if (p < 0)
        return "";

    p++;
    while (p < (int)json.length() &&
           (json[p] == ' ' || json[p] == '\r' ||
            json[p] == '\n' || json[p] == '\t'))
    {
        p++;
    }

    // A nullable nested object is valid JSON but means no object.
    if (json.substring(p).startsWith("null"))
        return "";

    if (p >= (int)json.length() || json[p] != '{')
        return "";

    const int start = p;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (; p < (int)json.length(); p++)
    {
        const char c = json[p];

        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '\"')
            {
                inString = false;
            }
            continue;
        }

        if (c == '\"')
        {
            inString = true;
            continue;
        }

        if (c == '{')
            depth++;
        else if (c == '}')
        {
            depth--;
            if (depth == 0)
                return json.substring(start, p + 1);
        }
    }

    return "";
}

static String jsonString(
    const String &json,
    const String &key)
{
    String pattern =
        "\"" + key + "\"";

    int p =
        json.indexOf(pattern);

    if (p < 0)
        return "";

    p = json.indexOf(':', p);

    if (p < 0)
        return "";

    p++;

    while (
        p < (int)json.length() &&
        (
            json[p] == ' ' ||
            json[p] == '\r' ||
            json[p] == '\n'))
    {
        p++;
    }

    if (
        p >= (int)json.length() ||
        json[p] != '"')
    {
        return "";
    }

    p++;

    int end = p;

    while (end < (int)json.length())
    {
        if (
            json[end] == '"' &&
            json[end - 1] != '\\')
        {
            break;
        }

        end++;
    }

    if (end >= (int)json.length())
        return "";

    return json.substring(p, end);
}

static long jsonLong(
    const String &json,
    const String &key,
    long defaultValue = 0)
{
    String pattern =
        "\"" + key + "\"";

    int p =
        json.indexOf(pattern);

    if (p < 0)
        return defaultValue;

    p = json.indexOf(':', p);

    if (p < 0)
        return defaultValue;

    p++;

    while (
        p < (int)json.length() &&
        (
            json[p] == ' ' ||
            json[p] == '\r' ||
            json[p] == '\n'))
    {
        p++;
    }

    int end = p;

    while (
        end < (int)json.length() &&
        (
            isDigit(json[end]) ||
            json[end] == '-'))
    {
        end++;
    }

    String value =
        json.substring(p, end);

    value.trim();

    if (value.length() == 0)
        return defaultValue;

    return value.toInt();
}

static bool jsonBool(
    const String &json,
    const String &key,
    bool defaultValue = false)
{
    String pattern =
        "\"" + key + "\"";

    int p =
        json.indexOf(pattern);

    if (p < 0)
        return defaultValue;

    p = json.indexOf(':', p);

    if (p < 0)
        return defaultValue;

    String tail =
        json.substring(p + 1);

    tail.trim();

    if (tail.startsWith("true"))
        return true;

    if (tail.startsWith("false"))
        return false;

    return defaultValue;
}

// ============================================================
// VEHICLE AUTHENTICATION
// ============================================================

VehicleAuthResult postVehicleAuth(
    const String &deviceId,
    const String &rfidUid,
    const String &pin)
{
    VehicleAuthResult result;

    result.requestOK = false;
    result.granted = false;
    result.reason = "";

    result.driverCode = "";
    result.fullName = "";
    result.vehicleCode = "";

    result.hasTrip = false;
    result.tripId = 0;
    result.tripCode = "";
    result.tripStatus = "";
    result.destinationName = "";

    Serial.println();
    Serial.println("================================");
    Serial.println(" AZURE VEHICLE AUTH");
    Serial.println("================================");

    Serial.print("DEVICE : ");
    Serial.println(deviceId);

    Serial.print("RFID   : ");
    Serial.println(rfidUid);

    if (
        deviceId.length() == 0 ||
        rfidUid.length() == 0 ||
        pin.length() == 0)
    {
        result.reason = "INVALID_ARGUMENT";
        return result;
    }

    String json = "{";

    json += "\"device_id\":\"";
    json += deviceId;
    json += "\",";

    json += "\"rfid_uid\":\"";
    json += rfidUid;
    json += "\",";

    json += "\"pin\":\"";
    json += pin;
    json += "\"";

    json += "}";

    Serial.print("[AUTH] Request length: ");
    Serial.println(json.length());

    HTTPResult http =
        httpPostJSON(
            AUTH_URL,
            json);

    if (!http.actionReceived)
    {
        result.reason =
            http.error.length() > 0
                ? http.error
                : "HTTP_REQUEST_FAILED";

        return result;
    }

    if (
        http.status >= 700 &&
        http.status <= 799)
    {
        result.reason =
            "MODEM_HTTP_ERROR_" +
            String(http.status);

        return result;
    }

    // HTTP request reached Azure.
    result.requestOK = true;

    if (
        http.status < 200 ||
        http.status >= 300)
    {
        result.granted = false;

        String backendReason =
            jsonString(
                http.body,
                "reason");

        if (backendReason.length() > 0)
            result.reason = backendReason;
        else
            result.reason =
                "HTTP_" +
                String(http.status);

        return result;
    }

    result.granted =
        jsonBool(
            http.body,
            "granted",
            false);

    result.reason =
        jsonString(
            http.body,
            "reason");

    // Azure returns nested objects:
    // {"driver":{...}, "trip":{...}, "vehicle":{...}}
    // Parse each object first so fields such as trip.id/status are
    // not confused with top-level or vehicle fields.
    const String driverObject =
        jsonObject(http.body, "driver");

    const String vehicleObject =
        jsonObject(http.body, "vehicle");

    const String tripObject =
        jsonObject(http.body, "trip");

    if (driverObject.length() > 0)
    {
        result.driverCode =
            jsonString(driverObject, "driver_code");

        result.fullName =
            jsonString(driverObject, "full_name");
    }

    if (vehicleObject.length() > 0)
    {
        result.vehicleCode =
            jsonString(vehicleObject, "vehicle_code");
    }

    if (tripObject.length() > 0)
    {
        result.tripId =
            jsonLong(tripObject, "id", 0);

        result.tripCode =
            jsonString(tripObject, "trip_code");

        result.tripStatus =
            jsonString(tripObject, "status");

        result.destinationName =
            jsonString(tripObject, "destination_name");

        result.hasTrip =
            result.tripId > 0;
    }
    else
    {
        result.hasTrip = false;
        result.tripId = 0;
    }

    if (
        result.reason.length() == 0 &&
        result.granted)
    {
        result.reason = "OK";
    }

    Serial.println();
    Serial.println("================================");

    if (result.granted)
    {
        Serial.println(" AUTH GRANTED");

        Serial.print(" DRIVER : ");
        Serial.println(result.driverCode);

        Serial.print(" NAME   : ");
        Serial.println(result.fullName);

        Serial.print(" VEHICLE: ");
        Serial.println(result.vehicleCode);

        if (result.hasTrip)
        {
            Serial.print(" TRIP   : ");
            Serial.println(result.tripCode);

            Serial.print(" STATUS : ");
            Serial.println(result.tripStatus);

            Serial.print(" DEST   : ");
            Serial.println(result.destinationName);
        }
        else
        {
            Serial.println(" TRIP   : NONE");
        }
    }
    else
    {
        Serial.println(" AUTH DENIED");

        Serial.print(" REASON : ");
        Serial.println(result.reason);
    }

    Serial.println("================================");

    return result;
}

// ============================================================
// START TRIP
// ============================================================

TripStartResult postStartTrip(
    const String &deviceId,
    long tripId)
{
    TripStartResult result;

    result.requestOK = false;
    result.started = false;
    result.alreadyStarted = false;
    result.reason = "";

    result.tripId = tripId;
    result.tripCode = "";
    result.tripStatus = "";
    result.originName = "";
    result.destinationName = "";
    result.startedAt = "";

    result.driverCode = "";
    result.fullName = "";

    result.vehicleCode = "";
    result.vehicleStatus = "";

    if (
        deviceId.length() == 0 ||
        tripId <= 0)
    {
        result.reason = "INVALID_ARGUMENT";
        return result;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println(" START TRIP");
    Serial.println("================================");

    String json = "{";

    json += "\"device_id\":\"";
    json += deviceId;
    json += "\",";

    json += "\"trip_id\":";
    json += String(tripId);

    json += "}";

    HTTPResult http =
        httpPostJSON(
            START_TRIP_URL,
            json);

    if (!http.actionReceived)
    {
        result.reason =
            http.error.length() > 0
                ? http.error
                : "HTTP_REQUEST_FAILED";

        return result;
    }

    if (
        http.status >= 700 &&
        http.status <= 799)
    {
        result.reason =
            "MODEM_HTTP_ERROR_" +
            String(http.status);

        return result;
    }

    result.requestOK = true;

    if (
        http.status < 200 ||
        http.status >= 300)
    {
        result.reason =
            jsonString(
                http.body,
                "reason");

        if (result.reason.length() == 0)
        {
            result.reason =
                "HTTP_" +
                String(http.status);
        }

        return result;
    }

    result.started =
        jsonBool(
            http.body,
            "started",
            true);

    result.alreadyStarted =
        jsonBool(
            http.body,
            "already_started",
            false);

    result.reason =
        jsonString(
            http.body,
            "reason");

    result.tripId =
        jsonLong(
            http.body,
            "trip_id",
            tripId);

    result.tripCode =
        jsonString(
            http.body,
            "trip_code");

    result.tripStatus =
        jsonString(
            http.body,
            "trip_status");

    result.originName =
        jsonString(
            http.body,
            "origin_name");

    result.destinationName =
        jsonString(
            http.body,
            "destination_name");

    result.startedAt =
        jsonString(
            http.body,
            "started_at");

    result.driverCode =
        jsonString(
            http.body,
            "driver_code");

    result.fullName =
        jsonString(
            http.body,
            "full_name");

    result.vehicleCode =
        jsonString(
            http.body,
            "vehicle_code");

    result.vehicleStatus =
        jsonString(
            http.body,
            "vehicle_status");

    if (result.reason.length() == 0)
        result.reason = "OK";

    return result;
}

// ============================================================
// GPS LOCATION -> AZURE
// ============================================================

GPSUploadResult postGPSLocation(
    const String &deviceId,
    long tripId,
    double latitude,
    double longitude,
    double speedKmh,
    uint32_t satellites)
{
    GPSUploadResult result;

    result.requestOK = false;
    result.saved = false;
    result.reason = "";
    result.gpsLogId = 0;

    if (deviceId.length() == 0)
    {
        result.reason =
            "INVALID_DEVICE_ID";

        return result;
    }

    if (tripId <= 0)
    {
        result.reason =
            "INVALID_TRIP_ID";

        return result;
    }

    if (
        latitude < -90.0 ||
        latitude > 90.0)
    {
        result.reason =
            "INVALID_LATITUDE";

        return result;
    }

    if (
        longitude < -180.0 ||
        longitude > 180.0)
    {
        result.reason =
            "INVALID_LONGITUDE";

        return result;
    }

    if (speedKmh < 0)
        speedKmh = 0;

    String json = "{";

    json += "\"device_id\":\"";
    json += deviceId;
    json += "\",";

    json += "\"trip_id\":";
    json += String(tripId);
    json += ",";

    json += "\"latitude\":";
    json += String(latitude, 6);
    json += ",";

    json += "\"longitude\":";
    json += String(longitude, 6);
    json += ",";

    json += "\"speed_kmh\":";
    json += String(speedKmh, 2);
    json += ",";

    json += "\"satellites\":";
    json += String(satellites);

    json += "}";

    Serial.println();
    Serial.println("================================");
    Serial.println(" AZURE GPS UPLOAD");
    Serial.println("================================");

    HTTPResult http =
        httpPostJSON(
            GPS_URL,
            json);

    if (!http.actionReceived)
    {
        result.reason =
            http.error.length() > 0
                ? http.error
                : "HTTP_REQUEST_FAILED";

        return result;
    }

    if (
        http.status >= 700 &&
        http.status <= 799)
    {
        result.reason =
            "MODEM_HTTP_ERROR_" +
            String(http.status);

        return result;
    }

    result.requestOK = true;

    if (
        http.status < 200 ||
        http.status >= 300)
    {
        result.reason =
            jsonString(
                http.body,
                "reason");

        if (result.reason.length() == 0)
        {
            result.reason =
                "HTTP_" +
                String(http.status);
        }

        return result;
    }

    result.saved =
        jsonBool(
            http.body,
            "saved",
            true);

    result.gpsLogId =
        jsonLong(
            http.body,
            "gps_log_id",
            0);

    result.reason =
        jsonString(
            http.body,
            "reason");

    if (result.reason.length() == 0)
        result.reason = "OK";

    return result;
}

// ============================================================
// ACCESS EVENT
// ============================================================

bool postAccessEvent(
    const String &deviceId,
    const String &event,
    const String &userId,
    const String &rfidUid,
    double latitude,
    double longitude)
{
    String json = "{";

    json += "\"device_id\":\"";
    json += deviceId;
    json += "\",";

    json += "\"event\":\"";
    json += event;
    json += "\",";

    json += "\"user_id\":\"";
    json += userId;
    json += "\",";

    json += "\"rfid_uid\":\"";
    json += rfidUid;
    json += "\",";

    json += "\"latitude\":";
    json += String(latitude, 6);
    json += ",";

    json += "\"longitude\":";
    json += String(longitude, 6);

    json += "}";

    HTTPResult http =
        httpPostJSON(
            ACCESS_URL,
            json);

    if (!http.actionReceived)
        return false;

    return (
        http.status >= 200 &&
        http.status < 300);
}

// ============================================================
// SEND SMS
// ============================================================

bool sendSMS(
    const char *phone,
    const String &message)
{
    Serial.println();
    Serial.println("================================");
    Serial.println(" SEND SMS");
    Serial.println("================================");

    if (
        phone == nullptr ||
        strlen(phone) == 0)
    {
        return false;
    }

    if (
        !sendATCommand(
            "AT",
            "OK",
            2000))
    {
        Serial.println("[SMS] MODEM NOT READY");
        return false;
    }

    if (
        !sendATCommand(
            "AT+CMGF=1",
            "OK",
            2000))
    {
        Serial.println("[SMS] TEXT MODE FAILED");
        return false;
    }

    clearGSMBuffer();

    Serial.print("[SMS] TO: ");
    Serial.println(phone);

    gsmSerial.print(
        "AT+CMGS=\"");

    gsmSerial.print(phone);

    gsmSerial.println("\"");

    String response;

    uint32_t start = millis();

    bool promptReady = false;

    while (millis() - start < 5000)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            Serial.write(c);

            response += c;

            if (c == '>')
            {
                promptReady = true;
                break;
            }

            if (
                response.indexOf("ERROR") >= 0)
            {
                return false;
            }
        }

        if (promptReady)
            break;

        delay(10);
    }

    if (!promptReady)
    {
        Serial.println("[SMS] NO > PROMPT");
        return false;
    }

    gsmSerial.print(message);

    delay(200);

    // CTRL+Z
    gsmSerial.write(26);

    response = "";

    start = millis();

    while (millis() - start < 20000)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();

            Serial.write(c);

            response += c;

            if (
                response.indexOf("+CMGS:") >= 0 &&
                response.indexOf("OK") >= 0)
            {
                Serial.println();
                Serial.println("[SMS] SEND SUCCESS");

                return true;
            }

            if (
                response.indexOf("ERROR") >= 0)
            {
                Serial.println();
                Serial.println("[SMS] SEND ERROR");

                return false;
            }
        }

        delay(20);
    }

    Serial.println("[SMS] SEND TIMEOUT");

    return false;
}

// ============================================================
// CREATE GPS SMS MESSAGE
// ============================================================

String createGPSMessage()
{
    String message;

    message += "CANH BAO\n";

    if (gpsValid)
    {
        message += "Vi tri hien tai:\n";

        message += "Lat: ";
        message += String(
            gpsLat,
            6);

        message += "\nLng: ";
        message += String(
            gpsLng,
            6);

        message += "\n";
    }
    else
    {
        message +=
            "Khong lay duoc vi tri GPS.";
    }

    return message;
}

// ============================================================
// SEND GPS SMS
// ============================================================

bool sendGPSSMS(
    const char *phone)
{
    if (!gpsValid)
    {
        Serial.println("[SMS] GPS INVALID");
        return false;
    }

    String message =
        createGPSMessage();

    return sendSMS(
        phone,
        message);
}

// ============================================================
// SEND GPS TO ALL PHONES
// ============================================================

bool sendGPSToAllPhones()
{
    if (!gpsValid)
    {
        Serial.println("[SMS] GPS NOT READY");
        return false;
    }

    String message =
        createGPSMessage();

    int successCount = 0;

    for (
        int i = 0;
        i < smsPhoneCount;
        i++)
    {
        // Ignore placeholder phone entries.
        if (
            String(smsPhones[i])
                .indexOf("x") >= 0)
        {
            continue;
        }

        if (
            sendSMS(
                smsPhones[i],
                message))
        {
            successCount++;
        }

        delay(3000);
    }

    return (
        successCount ==
        smsPhoneCount);
}

// ============================================================
// CAMERA JPEG UPLOAD - FAST / REUSED HTTP SESSION
// ============================================================
//
// The original implementation called HTTPTERM after every JPEG.
// That forces A7680C to create a new HTTP/TLS session for every
// frame and adds several seconds of latency.
//
// FAST mode keeps the HTTP context alive between successful
// camera frames. If another feature (GPS/auth/trip) has closed
// the context, the first camera command will fail and this code
// automatically rebuilds the session once.
// ============================================================

static bool cameraHttpSessionReady = false;

static void closeCameraHttpSession()
{
    sendATCommand(
        "AT+HTTPTERM",
        "",
        1500);

    cameraHttpSessionReady = false;
}

static bool openCameraHttpSession()
{
    if (cameraHttpSessionReady)
        return true;

    if (!prepareHTTPSession())
        return false;

    if (!configureAzureHTTPS())
    {
        closeCameraHttpSession();
        return false;
    }

    cameraHttpSessionReady = true;
    return true;
}

static bool configureCameraRequest(
    const String &deviceId,
    long tripId)
{
    String url =
        String(CAMERA_URL) +
        "?device_id=" +
        deviceId +
        "&trip_id=" +
        String(tripId);

    String urlCommand =
        "AT+HTTPPARA=\"URL\",\"" +
        url +
        "\"";

    if (!sendATCommand(
            urlCommand.c_str(),
            "OK",
            2500))
    {
        return false;
    }

    if (!sendATCommand(
            "AT+HTTPPARA=\"CONTENT\",\"image/jpeg\"",
            "OK",
            2500))
    {
        return false;
    }

    return true;
}

CameraUploadResult postCameraImage(
    const String &deviceId,
    long tripId,
    const uint8_t *jpegData,
    size_t jpegLength)
{
    CameraUploadResult result;

    result.requestOK = false;
    result.uploaded = false;
    result.httpStatus = 0;
    result.reason = "";

    if (
        deviceId.length() == 0 ||
        tripId <= 0 ||
        jpegData == nullptr ||
        jpegLength == 0)
    {
        result.reason = "INVALID_ARGUMENT";
        return result;
    }

    const uint32_t uploadStartedAt = millis();

    Serial.println();
    Serial.println("================================");
    Serial.println(" CAMERA FAST UPLOAD");
    Serial.println("================================");
    Serial.print("[CAMERA] JPEG : ");
    Serial.print(jpegLength);
    Serial.println(" bytes");

    // --------------------------------------------------------
    // REUSE HTTP/TLS SESSION
    // --------------------------------------------------------
    // Try the existing camera HTTP context first. If GPS/auth
    // closed it, rebuild once and retry the request parameters.
    // --------------------------------------------------------

    if (!openCameraHttpSession())
    {
        result.reason = "HTTP_SESSION_FAILED";
        return result;
    }

    if (!configureCameraRequest(deviceId, tripId))
    {
        Serial.println(
            "[CAMERA FAST] Session stale -> reconnect once.");

        closeCameraHttpSession();

        if (
            !openCameraHttpSession() ||
            !configureCameraRequest(deviceId, tripId))
        {
            result.reason = "HTTP_CONFIG_FAILED";
            closeCameraHttpSession();
            return result;
        }
    }

    // A 5-10 KB QVGA JPEG normally reaches DOWNLOAD quickly.
    // Keep a generous modem-side data timeout, while the local
    // wait is reduced from the previous 5 seconds.
    String dataCommand =
        "AT+HTTPDATA=" +
        String(jpegLength) +
        ",10000";

    if (!sendATCommand(
            dataCommand.c_str(),
            "DOWNLOAD",
            2500))
    {
        result.reason = "HTTPDATA_FAILED";
        closeCameraHttpSession();
        return result;
    }

    Serial.print("[CAMERA] Sending ");
    Serial.print(jpegLength);
    Serial.println(" bytes...");

    const size_t bytesSent =
        gsmSerial.write(jpegData, jpegLength);

    if (bytesSent != jpegLength)
    {
        result.reason = "UART_WRITE_INCOMPLETE";
        closeCameraHttpSession();
        return result;
    }

    // --------------------------------------------------------
    // WAIT FOR A7680C TO ACCEPT JPEG DATA
    // --------------------------------------------------------

    String response;
    response.reserve(128);

    uint32_t start = millis();
    bool dataOK = false;

    while (millis() - start < 12000)
    {
        while (gsmSerial.available() > 0)
        {
            char c = gsmSerial.read();
            Serial.write(c);
            response += c;

            if (response.indexOf("\r\nOK\r\n") >= 0)
            {
                dataOK = true;
                break;
            }

            if (response.indexOf("ERROR") >= 0)
                break;

            // Prevent a noisy modem response from growing the
            // Arduino String indefinitely.
            if (response.length() > 256)
                response.remove(0, 128);
        }

        if (dataOK || response.indexOf("ERROR") >= 0)
            break;

        delay(2);
    }

    if (!dataOK)
    {
        result.reason = "CAMERA_DATA_SEND_FAILED";
        closeCameraHttpSession();
        return result;
    }

    // --------------------------------------------------------
    // POST CURRENT JPEG
    // --------------------------------------------------------

    clearGSMBuffer();

    Serial.println();
    Serial.println("[GSM TX] AT+HTTPACTION=1");
    gsmSerial.println("AT+HTTPACTION=1");

    // The old code allowed 120 seconds. For live camera that can
    // freeze the whole loop for too long. 25 seconds still gives
    // 4G/Azure enough recovery room; a failure closes the session
    // and the next frame reconnects cleanly.
    HTTPResult http = waitHTTPAction(25000);

    result.httpStatus = http.status;

    if (!http.actionReceived)
    {
        result.reason =
            http.error.length() > 0
                ? http.error
                : "HTTP_REQUEST_FAILED";

        closeCameraHttpSession();
        return result;
    }

    if (http.status >= 700 && http.status <= 799)
    {
        result.reason =
            "MODEM_HTTP_ERROR_" +
            String(http.status);

        closeCameraHttpSession();
        return result;
    }

    result.requestOK = true;

    if (http.status >= 200 && http.status < 300)
    {
        result.uploaded = true;
        result.reason = "OK";

        // IMPORTANT: do NOT HTTPTERM here.
        // Keep the session for the next camera frame.
    }
    else
    {
        result.uploaded = false;
        result.reason = "HTTP_" + String(http.status);

        // Unknown server state: reconnect on next frame.
        closeCameraHttpSession();
    }

    Serial.print("[CAMERA FAST] Total upload time: ");
    Serial.print(millis() - uploadStartedAt);
    Serial.println(" ms");

    return result;
}

