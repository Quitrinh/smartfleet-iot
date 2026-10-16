#!/bin/bash

set -e

# ============================================================
# ESP32 LOGICTIC
# ESP32-S3-CAM
#
# AUTO:
#   1. Check Python
#   2. Create virtual environment
#   3. Check / install PlatformIO
#   4. Check project structure
#   5. Build firmware
#   6. Detect ESP32 serial port
#   7. Upload firmware
#   8. Open Serial Monitor
# ============================================================


# ============================================================
# PROJECT DIRECTORY
# ============================================================

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"


echo ""
echo "=========================================="
echo "          ESP32 LOGICTIC"
echo "          ESP32-S3-CAM"
echo "=========================================="

echo ""
echo "Project directory:"
echo "$PROJECT_DIR"


# ============================================================
# 1. CHECK PYTHON
# ============================================================

echo ""
echo "=========================================="
echo "[1/7] CHECK PYTHON"
echo "=========================================="

if ! command -v python3 >/dev/null 2>&1; then
    echo ""
    echo "[ERROR] Python 3 not found."
    echo ""
    echo "Please install Python 3 first."
    exit 1
fi

echo "[OK] Python 3 found:"
python3 --version


# ============================================================
# 2. PYTHON VIRTUAL ENVIRONMENT
# ============================================================

echo ""
echo "=========================================="
echo "[2/7] PYTHON ENVIRONMENT"
echo "=========================================="

if [ ! -d ".venv" ]; then
    echo ""
    echo "Virtual environment not found."
    echo "Creating .venv..."

    python3 -m venv .venv

    echo "[OK] .venv created"
else
    echo "[OK] .venv already exists"
fi


echo ""
echo "Activating .venv..."

source .venv/bin/activate


echo ""
echo "Python environment:"
python --version


# ============================================================
# 3. CHECK PLATFORMIO
# ============================================================

echo ""
echo "=========================================="
echo "[3/7] CHECK PLATFORMIO"
echo "=========================================="

if ! python -m platformio --version >/dev/null 2>&1; then

    echo ""
    echo "PlatformIO not found."
    echo "Installing PlatformIO..."

    python -m pip install --upgrade pip
    python -m pip install platformio

    echo ""
    echo "[OK] PlatformIO installed"

else

    echo "[OK] PlatformIO already installed"

fi


echo ""
python -m platformio --version


# ============================================================
# 4. CHECK PROJECT STRUCTURE
# ============================================================

echo ""
echo "=========================================="
echo "[4/7] CHECK PROJECT STRUCTURE"
echo "=========================================="


# ------------------------------------------------------------
# platformio.ini
# ------------------------------------------------------------

if [ ! -f "platformio.ini" ]; then
    echo ""
    echo "[ERROR] platformio.ini not found"
    echo ""
    echo "Expected:"
    echo "$PROJECT_DIR/platformio.ini"
    exit 1
fi

echo "[OK] platformio.ini"


# ------------------------------------------------------------
# firmware/
# ------------------------------------------------------------

if [ ! -d "firmware" ]; then
    echo "[ERROR] firmware/ not found"
    exit 1
fi

echo "[OK] firmware/"


# ------------------------------------------------------------
# firmware/src/
# ------------------------------------------------------------

if [ ! -d "firmware/src" ]; then
    echo "[ERROR] firmware/src/ not found"
    exit 1
fi

echo "[OK] firmware/src/"


# ------------------------------------------------------------
# firmware/include/
# ------------------------------------------------------------

if [ ! -d "firmware/include" ]; then
    echo "[ERROR] firmware/include/ not found"
    exit 1
fi

echo "[OK] firmware/include/"


# ------------------------------------------------------------
# main.cpp
# ------------------------------------------------------------

if [ ! -f "firmware/src/main.cpp" ]; then
    echo "[ERROR] firmware/src/main.cpp not found"
    exit 1
fi

echo "[OK] firmware/src/main.cpp"


# ------------------------------------------------------------
# Config
# ------------------------------------------------------------

if [ ! -f "firmware/include/config/pins.h" ]; then
    echo "[ERROR] firmware/include/config/pins.h not found"
    exit 1
fi

echo "[OK] config/pins.h"


if [ ! -f "firmware/include/config/settings.h" ]; then
    echo "[ERROR] firmware/include/config/settings.h not found"
    exit 1
fi

echo "[OK] config/settings.h"


# ------------------------------------------------------------
# Core
# ------------------------------------------------------------

if [ ! -f "firmware/include/core/system_bits.h" ]; then
    echo "[ERROR] firmware/include/core/system_bits.h not found"
    exit 1
fi

echo "[OK] core/system_bits.h"


# ------------------------------------------------------------
# Drivers
# ------------------------------------------------------------

if [ ! -f "firmware/include/drivers/camera.h" ]; then
    echo "[ERROR] firmware/include/drivers/camera.h not found"
    exit 1
fi

echo "[OK] drivers/camera.h"


if [ ! -f "firmware/include/drivers/lcd.h" ]; then
    echo "[ERROR] firmware/include/drivers/lcd.h not found"
    exit 1
fi

echo "[OK] drivers/lcd.h"


if [ ! -f "firmware/include/drivers/gps.h" ]; then
    echo "[ERROR] firmware/include/drivers/gps.h not found"
    exit 1
fi

echo "[OK] drivers/gps.h"


# ------------------------------------------------------------
# Network
# ------------------------------------------------------------

if [ ! -f "firmware/include/network/wifi_manager.h" ]; then
    echo "[ERROR] firmware/include/network/wifi_manager.h not found"
    exit 1
fi

echo "[OK] network/wifi_manager.h"


echo ""
echo "[OK] Project structure valid"


# ============================================================
# 5. BUILD FIRMWARE
# ============================================================

echo ""
echo "=========================================="
echo "[5/7] BUILD FIRMWARE"
echo "=========================================="

echo ""
echo "Building..."
echo ""

python -m platformio run


echo ""
echo "=========================================="
echo "             BUILD SUCCESS"
echo "=========================================="


# ============================================================
# 6. DETECT ESP32 SERIAL PORT
# ============================================================

echo ""
echo "=========================================="
echo "[6/7] DETECT ESP32"
echo "=========================================="

echo ""
echo "Searching serial ports..."


# ------------------------------------------------------------
# ESP32-S3 may appear as:
#
# /dev/cu.usbmodem2101
# /dev/cu.usbserial-xxxx
# /dev/cu.wchusbserialxxxx
# /dev/cu.SLAB_USBtoUART
# ------------------------------------------------------------

PORT=$(
    ls /dev/cu.* 2>/dev/null |
    grep -Ei "usbmodem|usbserial|wch|SLAB|serial" |
    head -n 1 || true
)


# ============================================================
# PORT NOT FOUND
# ============================================================

if [ -z "$PORT" ]; then

    echo ""
    echo "[ERROR] ESP32 serial port not found."

    echo ""
    echo "Available serial ports:"
    echo "------------------------------------------"

    ls /dev/cu.* 2>/dev/null || true

    echo "------------------------------------------"

    echo ""
    echo "Check:"
    echo "  1. USB data cable"
    echo "  2. ESP32-S3 USB connection"
    echo "  3. ESP32-S3 power"
    echo "  4. Try RESET"
    echo "  5. Try BOOT + RESET if necessary"

    exit 1
fi


echo ""
echo "[OK] ESP32 detected"
echo ""
echo "Port:"
echo "$PORT"


# ============================================================
# 7. UPLOAD
# ============================================================

echo ""
echo "=========================================="
echo "[7/7] UPLOAD FIRMWARE"
echo "=========================================="

echo ""
echo "Target port:"
echo "$PORT"

echo ""
echo "Uploading..."
echo ""


python -m platformio run \
    --target upload \
    --upload-port "$PORT"


echo ""
echo "=========================================="
echo "            UPLOAD SUCCESS"
echo "=========================================="


# ============================================================
# WAIT FOR ESP32 RESET
# ============================================================

echo ""
echo "Waiting for ESP32-S3 reset..."

sleep 3


# ============================================================
# CHECK PORT AGAIN
#
# ESP32-S3 native USB can disconnect/reconnect after upload.
# Therefore detect the port again.
# ============================================================

echo ""
echo "Checking serial port again..."


NEW_PORT=$(
    ls /dev/cu.* 2>/dev/null |
    grep -Ei "usbmodem|usbserial|wch|SLAB|serial" |
    head -n 1 || true
)


if [ -n "$NEW_PORT" ]; then
    PORT="$NEW_PORT"
fi


echo ""
echo "[OK] Serial port:"
echo "$PORT"


# ============================================================
# SERIAL MONITOR
# ============================================================

echo ""
echo "=========================================="
echo "            SERIAL MONITOR"
echo "=========================================="

echo ""
echo "Port : $PORT"
echo "Baud : 115200"

echo ""
echo "Expected modules:"
echo "  EventGroup"
echo "  LCD"
echo "  Camera"
echo "  PSRAM"
echo "  WiFi"
echo "  Camera Web Server"
echo "  GPS NEO-7M"

echo ""
echo "Press Ctrl+C to exit."
echo ""


python -m platformio device monitor \
    --port "$PORT" \
    --baud 115200