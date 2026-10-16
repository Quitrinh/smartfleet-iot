#!/bin/bash

set -e

echo ""
echo "=================================================="
echo "     CREATE ESP32 LOGICTIC PROJECT STRUCTURE"
echo "=================================================="
echo ""

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

echo "[INFO] Project:"
echo "$PROJECT_DIR"
echo ""

# ============================================================
# CREATE DIRECTORIES
# ============================================================

echo "[1/4] Creating directories..."

mkdir -p firmware/include/config
mkdir -p firmware/include/core
mkdir -p firmware/include/drivers
mkdir -p firmware/include/network
mkdir -p firmware/include/tasks

mkdir -p firmware/src/core
mkdir -p firmware/src/drivers
mkdir -p firmware/src/network
mkdir -p firmware/src/tasks

mkdir -p firmware/lib
mkdir -p firmware/test

mkdir -p backend
mkdir -p web
mkdir -p docs
mkdir -p dist

echo "[OK] Directories created"


# ============================================================
# CREATE HEADER FILES
# ============================================================

echo ""
echo "[2/4] Creating header files..."

touch firmware/include/config/pins.h
touch firmware/include/config/settings.h

touch firmware/include/core/globals.h
touch firmware/include/core/system_bits.h

touch firmware/include/drivers/camera.h
touch firmware/include/drivers/lcd.h
touch firmware/include/drivers/gps.h
touch firmware/include/drivers/gsm.h
touch firmware/include/drivers/rfid.h
touch firmware/include/drivers/keypad.h
touch firmware/include/drivers/output.h

touch firmware/include/network/wifi_manager.h
touch firmware/include/network/api_client.h

touch firmware/include/tasks/task_gps.h
touch firmware/include/tasks/task_gsm.h
touch firmware/include/tasks/task_auth.h
touch firmware/include/tasks/task_display.h

echo "[OK] Header files created"


# ============================================================
# CREATE SOURCE FILES
# ============================================================

echo ""
echo "[3/4] Creating source files..."

touch firmware/src/main.cpp

touch firmware/src/core/globals.cpp
touch firmware/src/core/system_bits.cpp

touch firmware/src/drivers/camera.cpp
touch firmware/src/drivers/lcd.cpp
touch firmware/src/drivers/gps.cpp
touch firmware/src/drivers/gsm.cpp
touch firmware/src/drivers/rfid.cpp
touch firmware/src/drivers/keypad.cpp
touch firmware/src/drivers/output.cpp

touch firmware/src/network/wifi_manager.cpp
touch firmware/src/network/api_client.cpp

touch firmware/src/tasks/task_gps.cpp
touch firmware/src/tasks/task_gsm.cpp
touch firmware/src/tasks/task_auth.cpp
touch firmware/src/tasks/task_display.cpp

echo "[OK] Source files created"


# ============================================================
# CREATE ROOT FILES
# ============================================================

echo ""
echo "[4/4] Creating project files..."

touch platformio.ini
touch setup.sh
touch README.md
touch .gitignore

touch backend/README.md
touch web/README.md
touch docs/README.md

echo "[OK] Root files created"


# ============================================================
# PERMISSIONS
# ============================================================

chmod +x create_structure.sh
chmod +x setup.sh


# ============================================================
# FINISH
# ============================================================

echo ""
echo "=================================================="
echo "          PROJECT STRUCTURE CREATED"
echo "=================================================="
echo ""

echo "ESP32_LOGICTIC/"
echo "├── firmware/"
echo "│   ├── include/"
echo "│   │   ├── config/"
echo "│   │   ├── core/"
echo "│   │   ├── drivers/"
echo "│   │   ├── network/"
echo "│   │   └── tasks/"
echo "│   ├── src/"
echo "│   │   ├── core/"
echo "│   │   ├── drivers/"
echo "│   │   ├── network/"
echo "│   │   └── tasks/"
echo "│   ├── lib/"
echo "│   └── test/"
echo "├── backend/"
echo "├── web/"
echo "├── docs/"
echo "├── dist/"
echo "├── platformio.ini"
echo "├── setup.sh"
echo "├── create_structure.sh"
echo "├── README.md"
echo "└── .gitignore"
echo ""
echo "[SUCCESS] Done."
echo ""