import os
from datetime import datetime, timezone

import psycopg
from psycopg.rows import dict_row
from flask import Flask, request, jsonify
from werkzeug.security import check_password_hash


app = Flask(__name__)


# ============================================================
# DATABASE
# ============================================================

def get_db_connection():
    """
    Create a secure connection to Azure PostgreSQL.

    Database credentials are read from Azure App Service
    Environment Variables instead of being hard-coded.
    """
    return psycopg.connect(
        host=os.environ["DB_HOST"],
        port=os.environ.get("DB_PORT", "5432"),
        dbname=os.environ["DB_NAME"],
        user=os.environ["DB_USER"],
        password=os.environ["DB_PASSWORD"],
        sslmode="require",
        row_factory=dict_row
    )


# ============================================================
# HOME
# ============================================================

@app.get("/")
def home():
    return jsonify({
        "project": "SmartFleet IoT",
        "service": "SmartFleet API",
        "status": "running"
    })


# ============================================================
# HEALTH
# ============================================================

@app.get("/health")
def health():
    return jsonify({
        "status": "ok",
        "service": "smartfleet-api"
    })


# ============================================================
# DATABASE HEALTH CHECK
# ============================================================

@app.get("/health/db")
def health_db():
    try:
        with get_db_connection() as conn:
            with conn.cursor() as cur:

                cur.execute("""
                    SELECT
                        current_database() AS database,
                        current_user AS db_user
                """)

                result = cur.fetchone()

        return jsonify({
            "status": "ok",
            "database": result["database"],
            "db_user": result["db_user"],
            "message": "PostgreSQL connection successful"
        })

    except Exception as e:
        print("DATABASE ERROR:", e)

        return jsonify({
            "status": "error",
            "message": "Database connection failed"
        }), 500


# ============================================================
# DATABASE SUMMARY
# ============================================================

@app.get("/api/db/summary")
def database_summary():
    try:
        with get_db_connection() as conn:
            with conn.cursor() as cur:

                cur.execute("SELECT COUNT(*) AS count FROM users")
                users = cur.fetchone()["count"]

                cur.execute(
                    "SELECT COUNT(*) AS count FROM driver_profiles"
                )
                drivers = cur.fetchone()["count"]

                cur.execute(
                    "SELECT COUNT(*) AS count FROM rfid_cards"
                )
                rfid_cards = cur.fetchone()["count"]

                cur.execute(
                    "SELECT COUNT(*) AS count FROM vehicles"
                )
                vehicles = cur.fetchone()["count"]

                cur.execute(
                    "SELECT COUNT(*) AS count FROM trips"
                )
                trips = cur.fetchone()["count"]

        return jsonify({
            "status": "ok",
            "data": {
                "users": users,
                "drivers": drivers,
                "rfid_cards": rfid_cards,
                "vehicles": vehicles,
                "trips": trips
            }
        })

    except Exception as e:
        print("DATABASE ERROR:", e)

        return jsonify({
            "status": "error",
            "message": "Could not read database"
        }), 500


# ============================================================
# HELPER - WRITE ACCESS LOG
# ============================================================

def write_access_log(
    cur,
    driver_id,
    vehicle_id,
    trip_id,
    rfid_uid,
    event,
    granted,
    reason
):
    cur.execute(
        """
        INSERT INTO access_logs (
            driver_id,
            vehicle_id,
            trip_id,
            rfid_uid,
            event,
            granted,
            reason
        )
        VALUES (
            %s, %s, %s, %s, %s, %s, %s
        )
        """,
        (
            driver_id,
            vehicle_id,
            trip_id,
            rfid_uid,
            event,
            granted,
            reason
        )
    )


# ============================================================
# VEHICLE AUTHENTICATION
#
# ESP32 sends:
#
# {
#     "device_id": "TRUCK_001",
#     "rfid_uid": "6B C8 99 8A",
#     "pin": "1234"
# }
#
# ============================================================

@app.post("/api/auth/vehicle")
def auth_vehicle():

    data = request.get_json(silent=True) or {}

    device_id = str(data.get("device_id", "")).strip()
    rfid_uid = str(data.get("rfid_uid", "")).strip().upper()
    pin = str(data.get("pin", "")).strip()

    # --------------------------------------------------------
    # Validate request
    # --------------------------------------------------------

    if not device_id or not rfid_uid or not pin:
        return jsonify({
            "status": "error",
            "granted": False,
            "reason": "MISSING_FIELDS",
            "message": "device_id, rfid_uid and pin are required"
        }), 400

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                # ====================================================
                # 1. CHECK VEHICLE
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        id,
                        vehicle_code,
                        device_id,
                        status
                    FROM vehicles
                    WHERE device_id = %s
                    """,
                    (device_id,)
                )

                vehicle = cur.fetchone()

                if vehicle is None:
                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "VEHICLE_NOT_FOUND"
                    }), 200

                if vehicle["status"] == "DISABLED":
                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "VEHICLE_DISABLED"
                    }), 200

                # ====================================================
                # 2. FIND RFID + DRIVER + USER
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        r.id AS rfid_id,
                        r.uid AS rfid_uid,
                        r.status AS rfid_status,

                        dp.id AS driver_id,
                        dp.driver_code,

                        u.id AS user_id,
                        u.full_name,
                        u.role,
                        u.status AS user_status,

                        p.pin_hash

                    FROM rfid_cards r

                    JOIN driver_profiles dp
                        ON dp.id = r.driver_id

                    JOIN users u
                        ON u.id = dp.user_id

                    LEFT JOIN driver_pins p
                        ON p.driver_id = dp.id

                    WHERE UPPER(r.uid) = %s
                    """,
                    (rfid_uid,)
                )

                driver = cur.fetchone()

                # ====================================================
                # 3. RFID NOT FOUND
                # ====================================================

                if driver is None:

                    write_access_log(
                        cur=cur,
                        driver_id=None,
                        vehicle_id=vehicle["id"],
                        trip_id=None,
                        rfid_uid=rfid_uid,
                        event="VEHICLE_AUTH",
                        granted=False,
                        reason="RFID_NOT_FOUND"
                    )

                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "RFID_NOT_FOUND"
                    }), 200

                # ====================================================
                # 4. RFID STATUS
                # ====================================================

                if driver["rfid_status"] != "ACTIVE":

                    write_access_log(
                        cur=cur,
                        driver_id=driver["driver_id"],
                        vehicle_id=vehicle["id"],
                        trip_id=None,
                        rfid_uid=rfid_uid,
                        event="VEHICLE_AUTH",
                        granted=False,
                        reason="RFID_NOT_ACTIVE"
                    )

                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "RFID_NOT_ACTIVE"
                    }), 200

                # ====================================================
                # 5. CHECK USER ROLE
                # ====================================================

                if driver["role"] != "DRIVER":

                    write_access_log(
                        cur=cur,
                        driver_id=driver["driver_id"],
                        vehicle_id=vehicle["id"],
                        trip_id=None,
                        rfid_uid=rfid_uid,
                        event="VEHICLE_AUTH",
                        granted=False,
                        reason="NOT_DRIVER"
                    )

                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "NOT_DRIVER"
                    }), 200

                # ====================================================
                # 6. CHECK DRIVER ACCOUNT STATUS
                # ====================================================

                if driver["user_status"] != "ACTIVE":

                    write_access_log(
                        cur=cur,
                        driver_id=driver["driver_id"],
                        vehicle_id=vehicle["id"],
                        trip_id=None,
                        rfid_uid=rfid_uid,
                        event="VEHICLE_AUTH",
                        granted=False,
                        reason="DRIVER_NOT_ACTIVE"
                    )

                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "DRIVER_NOT_ACTIVE"
                    }), 200

                # ====================================================
                # 7. CHECK WHETHER PIN EXISTS
                # ====================================================

                if not driver["pin_hash"]:

                    write_access_log(
                        cur=cur,
                        driver_id=driver["driver_id"],
                        vehicle_id=vehicle["id"],
                        trip_id=None,
                        rfid_uid=rfid_uid,
                        event="VEHICLE_AUTH",
                        granted=False,
                        reason="PIN_NOT_CONFIGURED"
                    )

                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "PIN_NOT_CONFIGURED"
                    }), 200

                # ====================================================
                # 8. VERIFY PIN HASH
                # ====================================================

                pin_valid = check_password_hash(
                    driver["pin_hash"],
                    pin
                )

                if not pin_valid:

                    write_access_log(
                        cur=cur,
                        driver_id=driver["driver_id"],
                        vehicle_id=vehicle["id"],
                        trip_id=None,
                        rfid_uid=rfid_uid,
                        event="VEHICLE_AUTH",
                        granted=False,
                        reason="INVALID_PIN"
                    )

                    return jsonify({
                        "status": "ok",
                        "granted": False,
                        "reason": "INVALID_PIN"
                    }), 200

                # ====================================================
                # 9. CHECK ACTIVE/ASSIGNED TRIP
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        id,
                        trip_code,
                        status,
                        destination_name
                    FROM trips
                    WHERE driver_id = %s
                      AND vehicle_id = %s
                      AND status IN (
                          'ASSIGNED',
                          'READY',
                          'IN_PROGRESS'
                      )
                    ORDER BY created_at DESC
                    LIMIT 1
                    """,
                    (
                        driver["driver_id"],
                        vehicle["id"]
                    )
                )

                trip = cur.fetchone()

                trip_id = trip["id"] if trip else None

                # ====================================================
                # 10. AUTH SUCCESS
                # ====================================================

                write_access_log(
                    cur=cur,
                    driver_id=driver["driver_id"],
                    vehicle_id=vehicle["id"],
                    trip_id=trip_id,
                    rfid_uid=rfid_uid,
                    event="VEHICLE_AUTH",
                    granted=True,
                    reason="AUTHENTICATED"
                )

                response = {
                    "status": "ok",
                    "granted": True,

                    "driver": {
                        "user_id": driver["user_id"],
                        "driver_id": driver["driver_id"],
                        "driver_code": driver["driver_code"],
                        "full_name": driver["full_name"]
                    },

                    "vehicle": {
                        "id": vehicle["id"],
                        "vehicle_code": vehicle["vehicle_code"],
                        "device_id": vehicle["device_id"],
                        "status": vehicle["status"]
                    }
                }

                # Trip is optional at authentication stage.
                if trip:

                    response["trip"] = {
                        "id": trip["id"],
                        "trip_code": trip["trip_code"],
                        "status": trip["status"],
                        "destination_name":
                            trip["destination_name"]
                    }

                else:

                    response["trip"] = None

                return jsonify(response), 200

    except Exception as e:

        print("AUTH DATABASE ERROR:", e)

        return jsonify({
            "status": "error",
            "granted": False,
            "reason": "SERVER_ERROR",
            "message": "Vehicle authentication failed"
        }), 500


# ============================================================
# ACCESS EVENT
# Existing endpoint - keep for compatibility with ESP32
# ============================================================

@app.post("/api/access")
def access():

    data = request.get_json(silent=True) or {}

    return jsonify({
        "status": "ok",
        "message": "Access event received",
        "received": data,
        "timestamp": datetime.now(timezone.utc).isoformat()
    })


# ============================================================
# GPS
# Existing endpoint
# ============================================================

@app.post("/api/gps")
def gps():

    data = request.get_json(silent=True) or {}

    return jsonify({
        "status": "ok",
        "message": "GPS data received",
        "received": data,
        "timestamp": datetime.now(timezone.utc).isoformat()
    })


# ============================================================
# LOCAL RUN
# ============================================================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=8000
    )