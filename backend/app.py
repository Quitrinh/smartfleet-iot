import os
from datetime import datetime, timezone

import psycopg
from psycopg.rows import dict_row
from flask import Flask, request, jsonify, render_template
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
# WEB DASHBOARD
# ============================================================

@app.get("/dashboard")
def dashboard():
    return render_template("dashboard.html")

# ============================================================
# WEB - VEHICLES
# ============================================================

@app.get("/vehicles")
def vehicles_page():
    return render_template("vehicles.html")

# ============================================================
# WEB - DRIVERS
# ============================================================

@app.get("/drivers")
def drivers_page():
    return render_template("drivers.html")


# ============================================================
# GET ALL DRIVERS
# ============================================================

@app.get("/api/drivers")
def get_all_drivers():

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                cur.execute(
                    """
                    SELECT
                        dp.id,
                        dp.driver_code,

                        u.id AS user_id,
                        u.full_name,
                        u.status,

                        t.id AS trip_id,
                        t.trip_code,
                        t.status AS trip_status,

                        v.id AS vehicle_id,
                        v.vehicle_code,
                        v.device_id,
                        v.status AS vehicle_status

                    FROM driver_profiles dp

                    JOIN users u
                        ON u.id = dp.user_id

                    LEFT JOIN trips t
                        ON t.driver_id = dp.id
                        AND t.status = 'IN_PROGRESS'

                    LEFT JOIN vehicles v
                        ON v.id = t.vehicle_id

                    ORDER BY dp.id ASC
                    """
                )

                rows = cur.fetchall()

        drivers = []

        for row in rows:

            drivers.append({
                "id": row["id"],
                "user_id": row["user_id"],

                "driver_code": row["driver_code"],
                "full_name": row["full_name"],
                "status": row["status"],

                "trip_id": row["trip_id"],
                "trip_code": row["trip_code"],
                "trip_status": row["trip_status"],

                "vehicle_id": row["vehicle_id"],
                "vehicle_code": row["vehicle_code"],
                "device_id": row["device_id"],
                "vehicle_status": row["vehicle_status"]
            })

        return jsonify({
            "status": "ok",
            "count": len(drivers),
            "drivers": drivers
        }), 200

    except Exception as e:

        print("[GET ALL DRIVERS ERROR]", repr(e))

        return jsonify({
            "status": "error",
            "reason": "SERVER_ERROR",
            "message": "Could not read drivers"
        }), 500

# ============================================================
# WEB - TRIPS
# ============================================================

@app.get("/trips")
def trips_page():
    return render_template("trips.html")
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
# GET ALL VEHICLES
# ============================================================

@app.get("/api/vehicles")
def get_all_vehicles():

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                cur.execute(
                    """
                    SELECT
                        v.id,
                        v.vehicle_code,
                        v.device_id,
                        v.status,

                        t.id AS trip_id,
                        t.trip_code,
                        t.status AS trip_status,

                        COUNT(g.id) AS gps_count,
                        MAX(g.recorded_at) AS last_gps_at

                    FROM vehicles v

                    LEFT JOIN trips t
                        ON t.vehicle_id = v.id
                        AND t.status = 'IN_PROGRESS'

                    LEFT JOIN gps_logs g
                        ON g.vehicle_id = v.id

                    GROUP BY
                        v.id,
                        v.vehicle_code,
                        v.device_id,
                        v.status,
                        t.id,
                        t.trip_code,
                        t.status

                    ORDER BY v.id ASC
                    """
                )

                rows = cur.fetchall()

        vehicles = []

        for row in rows:

            vehicles.append({
                "id": row["id"],
                "vehicle_code": row["vehicle_code"],
                "device_id": row["device_id"],
                "status": row["status"],

                "trip_id": row["trip_id"],
                "trip_code": row["trip_code"],
                "trip_status": row["trip_status"],

                "gps_count": row["gps_count"],

                "last_gps_at":
                    row["last_gps_at"].isoformat()
                    if row["last_gps_at"]
                    else None
            })

        return jsonify({
            "status": "ok",
            "count": len(vehicles),
            "vehicles": vehicles
        }), 200

    except Exception as e:

        print(
            "[GET ALL VEHICLES ERROR]",
            repr(e)
        )

        return jsonify({
            "status": "error",
            "reason": "SERVER_ERROR",
            "message": "Could not read vehicles"
        }), 500
# ============================================================
# START TRIP
#
# ESP32 / Postman sends:
#
# {
#     "device_id": "TRUCK_001",
#     "trip_id": 1
# }
#
# ============================================================

@app.post("/api/trips/start")
def start_trip():

    data = request.get_json(silent=True) or {}

    device_id = str(data.get("device_id", "")).strip()

    try:
        trip_id = int(data.get("trip_id"))
    except (TypeError, ValueError):
        trip_id = 0

    # --------------------------------------------------------
    # 1. VALIDATE REQUEST
    # --------------------------------------------------------

    if not device_id or trip_id <= 0:

        return jsonify({
            "status": "error",
            "started": False,
            "reason": "MISSING_FIELDS",
            "message": "device_id and trip_id are required"
        }), 400

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                # ====================================================
                # 2. FIND VEHICLE
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
                    FOR UPDATE
                    """,
                    (device_id,)
                )

                vehicle = cur.fetchone()

                if vehicle is None:

                    return jsonify({
                        "status": "ok",
                        "started": False,
                        "reason": "VEHICLE_NOT_FOUND"
                    }), 200

                if vehicle["status"] == "DISABLED":

                    return jsonify({
                        "status": "ok",
                        "started": False,
                        "reason": "VEHICLE_DISABLED"
                    }), 200

                # ====================================================
                # 3. FIND TRIP
                #
                # IMPORTANT:
                # Trip must belong to the vehicle that sent request.
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        t.id,
                        t.trip_code,
                        t.driver_id,
                        t.vehicle_id,
                        t.origin_name,
                        t.destination_name,
                        t.status,
                        t.started_at,

                        dp.driver_code,

                        u.full_name

                    FROM trips t

                    JOIN driver_profiles dp
                        ON dp.id = t.driver_id

                    JOIN users u
                        ON u.id = dp.user_id

                    WHERE t.id = %s
                      AND t.vehicle_id = %s

                    FOR UPDATE
                    """,
                    (
                        trip_id,
                        vehicle["id"]
                    )
                )

                trip = cur.fetchone()

                if trip is None:

                    return jsonify({
                        "status": "ok",
                        "started": False,
                        "reason": "TRIP_NOT_FOUND"
                    }), 200

                # ====================================================
                # 4. HANDLE TRIP STATUS
                # ====================================================

                # Idempotent behavior:
                # if already started, do not start it a second time.
                if trip["status"] == "IN_PROGRESS":

                    return jsonify({
                        "status": "ok",
                        "started": True,
                        "already_started": True,

                        "trip": {
                            "id": trip["id"],
                            "trip_code": trip["trip_code"],
                            "status": trip["status"],
                            "origin_name": trip["origin_name"],
                            "destination_name":
                                trip["destination_name"],
                            "started_at":
                                trip["started_at"].isoformat()
                                if trip["started_at"]
                                else None
                        },

                        "driver": {
                            "driver_code": trip["driver_code"],
                            "full_name": trip["full_name"]
                        },

                        "vehicle": {
                            "id": vehicle["id"],
                            "vehicle_code":
                                vehicle["vehicle_code"],
                            "device_id":
                                vehicle["device_id"],
                            "status":
                                vehicle["status"]
                        }

                    }), 200

                # Only ASSIGNED or READY can start.
                if trip["status"] not in (
                    "ASSIGNED",
                    "READY"
                ):

                    return jsonify({
                        "status": "ok",
                        "started": False,
                        "reason": "INVALID_TRIP_STATUS",
                        "trip_status": trip["status"]
                    }), 200

                # Vehicle must be available before starting.
                if vehicle["status"] != "AVAILABLE":

                    return jsonify({
                        "status": "ok",
                        "started": False,
                        "reason": "VEHICLE_NOT_AVAILABLE",
                        "vehicle_status": vehicle["status"]
                    }), 200

                # ====================================================
                # 5. UPDATE TRIP
                #
                # ASSIGNED / READY
                #       ↓
                # IN_PROGRESS
                # ====================================================

                cur.execute(
                    """
                    UPDATE trips
                    SET
                        status = 'IN_PROGRESS',
                        started_at = CURRENT_TIMESTAMP
                    WHERE id = %s
                    RETURNING
                        id,
                        trip_code,
                        status,
                        started_at
                    """,
                    (trip["id"],)
                )

                updated_trip = cur.fetchone()

                # ====================================================
                # 6. UPDATE VEHICLE
                #
                # AVAILABLE
                #     ↓
                # IN_TRIP
                # ====================================================

                cur.execute(
                    """
                    UPDATE vehicles
                    SET status = 'IN_TRIP'
                    WHERE id = %s
                    RETURNING
                        id,
                        vehicle_code,
                        device_id,
                        status
                    """,
                    (vehicle["id"],)
                )

                updated_vehicle = cur.fetchone()

                # ====================================================
                # 7. WRITE ACCESS LOG
                # ====================================================

                write_access_log(
                    cur=cur,
                    driver_id=trip["driver_id"],
                    vehicle_id=vehicle["id"],
                    trip_id=trip["id"],
                    rfid_uid=None,
                    event="TRIP_STARTED",
                    granted=True,
                    reason="STARTED"
                )

                # Connection context commits transaction on success.

                # ====================================================
                # 8. RESPONSE
                # ====================================================

                return jsonify({

                    "status": "ok",
                    "started": True,
                    "already_started": False,

                    "trip": {
                        "id": updated_trip["id"],
                        "trip_code":
                            updated_trip["trip_code"],
                        "status":
                            updated_trip["status"],
                        "origin_name":
                            trip["origin_name"],
                        "destination_name":
                            trip["destination_name"],
                        "started_at":
                            updated_trip["started_at"].isoformat()
                            if updated_trip["started_at"]
                            else None
                    },

                    "driver": {
                        "driver_code":
                            trip["driver_code"],
                        "full_name":
                            trip["full_name"]
                    },

                    "vehicle": {
                        "id":
                            updated_vehicle["id"],
                        "vehicle_code":
                            updated_vehicle["vehicle_code"],
                        "device_id":
                            updated_vehicle["device_id"],
                        "status":
                            updated_vehicle["status"]
                    }

                }), 200

    except Exception as e:

        print("START TRIP DATABASE ERROR:", e)

        return jsonify({
            "status": "error",
            "started": False,
            "reason": "SERVER_ERROR",
            "message": "Could not start trip"
        }), 500


# ============================================================
# GET ALL TRIPS
# ============================================================

@app.get("/api/trips")
def get_all_trips():

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                cur.execute(
                    """
                    SELECT
                        t.id,
                        t.trip_code,
                        t.status,
                        t.origin_name,
                        t.destination_name,
                        t.started_at,

                        v.id AS vehicle_id,
                        v.vehicle_code,
                        v.device_id,
                        v.status AS vehicle_status,

                        dp.id AS driver_id,
                        dp.driver_code,

                        u.full_name AS driver_name,

                        COUNT(g.id) AS gps_count

                    FROM trips t

                    JOIN vehicles v
                        ON v.id = t.vehicle_id

                    JOIN driver_profiles dp
                        ON dp.id = t.driver_id

                    JOIN users u
                        ON u.id = dp.user_id

                    LEFT JOIN gps_logs g
                        ON g.trip_id = t.id

                    GROUP BY
                        t.id,
                        t.trip_code,
                        t.status,
                        t.origin_name,
                        t.destination_name,
                        t.started_at,

                        v.id,
                        v.vehicle_code,
                        v.device_id,
                        v.status,

                        dp.id,
                        dp.driver_code,

                        u.full_name

                    ORDER BY t.id DESC
                    """
                )

                rows = cur.fetchall()

        trips = []

        for row in rows:

            trips.append({
                "id": row["id"],
                "trip_code": row["trip_code"],
                "status": row["status"],

                "origin_name": row["origin_name"],
                "destination_name": row["destination_name"],

                "started_at":
                    row["started_at"].isoformat()
                    if row["started_at"]
                    else None,

                "vehicle_id": row["vehicle_id"],
                "vehicle_code": row["vehicle_code"],
                "device_id": row["device_id"],
                "vehicle_status": row["vehicle_status"],

                "driver_id": row["driver_id"],
                "driver_code": row["driver_code"],
                "driver_name": row["driver_name"],

                "gps_count": row["gps_count"]
            })

        return jsonify({
            "status": "ok",
            "count": len(trips),
            "trips": trips
        }), 200


    except Exception as e:

        print(
            "[GET ALL TRIPS ERROR]",
            repr(e)
        )

        return jsonify({
            "status": "error",
            "reason": "SERVER_ERROR",
            "message": "Could not read trips"
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
# GPS LOCATION
# ============================================================
#
# ESP32 sends:
#
# {
#     "device_id": "TRUCK_001",
#     "trip_id": 1,
#     "latitude": 10.75,
#     "longitude": 106.67,
#     "speed_kmh": 25.5,
#     "satellites": 8
# }
#
# Backend:
#
# 1. Validate JSON
# 2. Validate data types
# 3. Validate GPS coordinates
# 4. Find vehicle using device_id
# 5. Find trip using trip_id
# 6. Verify trip belongs to vehicle
# 7. Verify trip is IN_PROGRESS
# 8. Verify vehicle is IN_TRIP
# 9. Insert into gps_logs
#
# ============================================================

@app.post("/api/gps")
def save_gps_location():

    # ========================================================
    # 1. READ JSON
    # ========================================================

    data = request.get_json(silent=True)

    if not data:

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "INVALID_JSON"
        }), 400


    # ========================================================
    # 2. GET FIELDS
    # ========================================================

    device_id = str(
        data.get("device_id", "")
    ).strip()

    trip_id = data.get("trip_id")

    latitude = data.get("latitude")

    longitude = data.get("longitude")

    # Optional fields
    speed_kmh = data.get("speed_kmh")

    satellites = data.get("satellites")


    # ========================================================
    # 3. REQUIRED FIELDS
    # ========================================================

    if (
        not device_id
        or trip_id is None
        or latitude is None
        or longitude is None
    ):

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "MISSING_FIELDS",
            "message":
                "device_id, trip_id, latitude and longitude are required"
        }), 400


    # ========================================================
    # 4. CONVERT DATA TYPES
    # ========================================================

    try:

        trip_id = int(trip_id)

        latitude = float(latitude)

        longitude = float(longitude)

        if speed_kmh is not None:
            speed_kmh = float(speed_kmh)

        if satellites is not None:
            satellites = int(satellites)

    except (TypeError, ValueError):

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "INVALID_DATA_TYPE"
        }), 400


    # ========================================================
    # 5. VALIDATE TRIP ID
    # ========================================================

    if trip_id <= 0:

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "INVALID_TRIP_ID"
        }), 400


    # ========================================================
    # 6. VALIDATE LATITUDE
    # ========================================================

    if not (-90.0 <= latitude <= 90.0):

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "INVALID_LATITUDE"
        }), 400


    # ========================================================
    # 7. VALIDATE LONGITUDE
    # ========================================================

    if not (-180.0 <= longitude <= 180.0):

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "INVALID_LONGITUDE"
        }), 400


    # ========================================================
    # 8. VALIDATE SPEED
    # ========================================================

    if speed_kmh is not None and speed_kmh < 0:

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "INVALID_SPEED"
        }), 400


    # ========================================================
    # 9. VALIDATE SATELLITES
    # ========================================================

    if satellites is not None and satellites < 0:

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "INVALID_SATELLITES"
        }), 400


    # ========================================================
    # 10. DATABASE
    # ========================================================

    try:

        # IMPORTANT:
        # Use the same psycopg connection as Auth and Start Trip.
        #
        # Do NOT use:
        #
        # with engine.begin() as conn:
        #
        # here.

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                # =================================================
                # 11. FIND VEHICLE
                # =================================================

                cur.execute(
                    """
                    SELECT
                        id,
                        vehicle_code,
                        device_id,
                        status
                    FROM vehicles
                    WHERE device_id = %s
                    LIMIT 1
                    """,
                    (device_id,)
                )

                vehicle = cur.fetchone()


                if vehicle is None:

                    return jsonify({
                        "status": "ok",
                        "saved": False,
                        "reason": "VEHICLE_NOT_FOUND"
                    }), 200


                # =================================================
                # 12. CHECK VEHICLE STATUS
                # =================================================

                if vehicle["status"] == "DISABLED":

                    return jsonify({
                        "status": "ok",
                        "saved": False,
                        "reason": "VEHICLE_DISABLED"
                    }), 200


                # =================================================
                # 13. FIND TRIP
                # =================================================

                cur.execute(
                    """
                    SELECT
                        id,
                        trip_code,
                        driver_id,
                        vehicle_id,
                        origin_name,
                        destination_name,
                        status,
                        started_at
                    FROM trips
                    WHERE id = %s
                    LIMIT 1
                    """,
                    (trip_id,)
                )

                trip = cur.fetchone()


                if trip is None:

                    return jsonify({
                        "status": "ok",
                        "saved": False,
                        "reason": "TRIP_NOT_FOUND"
                    }), 200


                # =================================================
                # 14. VERIFY TRIP -> VEHICLE
                # =================================================

                if trip["vehicle_id"] != vehicle["id"]:

                    return jsonify({
                        "status": "ok",
                        "saved": False,
                        "reason": "TRIP_VEHICLE_MISMATCH"
                    }), 200


                # =================================================
                # 15. TRIP MUST BE IN_PROGRESS
                # =================================================

                if trip["status"] != "IN_PROGRESS":

                    return jsonify({
                        "status": "ok",
                        "saved": False,
                        "reason": "TRIP_NOT_IN_PROGRESS",
                        "trip_status": trip["status"]
                    }), 200


                # =================================================
                # 16. VEHICLE MUST BE IN_TRIP
                # =================================================

                if vehicle["status"] != "IN_TRIP":

                    return jsonify({
                        "status": "ok",
                        "saved": False,
                        "reason": "VEHICLE_NOT_IN_TRIP",
                        "vehicle_status": vehicle["status"]
                    }), 200


                # =================================================
                # 17. INSERT GPS LOG
                # =================================================

                cur.execute(
                    """
                    INSERT INTO gps_logs (
                        trip_id,
                        vehicle_id,
                        latitude,
                        longitude,
                        speed_kmh,
                        satellites
                    )
                    VALUES (
                        %s,
                        %s,
                        %s,
                        %s,
                        %s,
                        %s
                    )
                    RETURNING
                        id,
                        trip_id,
                        vehicle_id,
                        latitude,
                        longitude,
                        speed_kmh,
                        satellites,
                        recorded_at
                    """,
                    (
                        trip["id"],
                        vehicle["id"],
                        latitude,
                        longitude,
                        speed_kmh,
                        satellites
                    )
                )

                gps_log = cur.fetchone()


            # =====================================================
            # psycopg connection context commits automatically
            # when no exception occurs.
            # =====================================================


        # ========================================================
        # 18. SUCCESS RESPONSE
        # ========================================================

        return jsonify({

            "status": "ok",

            "saved": True,

            "gps_log": {

                "id":
                    gps_log["id"],

                "trip_id":
                    gps_log["trip_id"],

                "vehicle_id":
                    gps_log["vehicle_id"],

                "latitude":
                    gps_log["latitude"],

                "longitude":
                    gps_log["longitude"],

                "speed_kmh":
                    gps_log["speed_kmh"],

                "satellites":
                    gps_log["satellites"],

                "recorded_at":
                    gps_log["recorded_at"].isoformat()
                    if gps_log["recorded_at"]
                    else None
            },

            "trip": {

                "id":
                    trip["id"],

                "trip_code":
                    trip["trip_code"],

                "status":
                    trip["status"],

                "origin_name":
                    trip["origin_name"],

                "destination_name":
                    trip["destination_name"]
            },

            "vehicle": {

                "id":
                    vehicle["id"],

                "vehicle_code":
                    vehicle["vehicle_code"],

                "device_id":
                    vehicle["device_id"],

                "status":
                    vehicle["status"]
            }

        }), 200


    # ========================================================
    # 19. SERVER ERROR
    # ========================================================

    except Exception as e:

        # Print actual exception to Azure logs.
        # Do not return database internals to production clients.
        print(
            "[GPS DATABASE ERROR]",
            repr(e)
        )

        return jsonify({
            "status": "error",
            "saved": False,
            "reason": "SERVER_ERROR",
            "message": "Could not save GPS location"
        }), 500

# ============================================================
# GET LATEST VEHICLE LOCATION
# ============================================================

@app.get("/api/vehicles/<device_id>/location")
def get_vehicle_location(device_id):

    device_id = str(device_id).strip()

    if not device_id:
        return jsonify({
            "status": "error",
            "reason": "INVALID_DEVICE_ID"
        }), 400

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                # ------------------------------------------------
                # FIND VEHICLE
                # ------------------------------------------------

                cur.execute(
                    """
                    SELECT
                        id,
                        vehicle_code,
                        device_id,
                        status
                    FROM vehicles
                    WHERE device_id = %s
                    LIMIT 1
                    """,
                    (device_id,)
                )

                vehicle = cur.fetchone()

                if vehicle is None:
                    return jsonify({
                        "status": "ok",
                        "found": False,
                        "reason": "VEHICLE_NOT_FOUND"
                    }), 200


                # ------------------------------------------------
                # GET LATEST GPS
                # ------------------------------------------------

                cur.execute(
                    """
                    SELECT
                        g.id,
                        g.trip_id,
                        g.latitude,
                        g.longitude,
                        g.speed_kmh,
                        g.satellites,
                        g.recorded_at,

                        t.trip_code,
                        t.status AS trip_status,
                        t.origin_name,
                        t.destination_name

                    FROM gps_logs g

                    JOIN trips t
                        ON t.id = g.trip_id

                    WHERE g.vehicle_id = %s

                    ORDER BY g.recorded_at DESC, g.id DESC

                    LIMIT 1
                    """,
                    (vehicle["id"],)
                )

                gps = cur.fetchone()


        if gps is None:

            return jsonify({
                "status": "ok",
                "found": False,

                "vehicle": {
                    "id": vehicle["id"],
                    "vehicle_code": vehicle["vehicle_code"],
                    "device_id": vehicle["device_id"],
                    "status": vehicle["status"]
                },

                "reason": "NO_GPS_DATA"

            }), 200


        return jsonify({

            "status": "ok",
            "found": True,

            "vehicle": {
                "id": vehicle["id"],
                "vehicle_code": vehicle["vehicle_code"],
                "device_id": vehicle["device_id"],
                "status": vehicle["status"]
            },

            "trip": {
                "id": gps["trip_id"],
                "trip_code": gps["trip_code"],
                "status": gps["trip_status"],
                "origin_name": gps["origin_name"],
                "destination_name": gps["destination_name"]
            },

            "location": {
                "gps_log_id": gps["id"],
                "latitude": gps["latitude"],
                "longitude": gps["longitude"],
                "speed_kmh": gps["speed_kmh"],
                "satellites": gps["satellites"],
                "recorded_at":
                    gps["recorded_at"].isoformat()
                    if gps["recorded_at"]
                    else None
            }

        }), 200


    except Exception as e:

        print(
            "[GET VEHICLE LOCATION ERROR]",
            repr(e)
        )

        return jsonify({
            "status": "error",
            "reason": "SERVER_ERROR",
            "message": "Could not read vehicle location"
        }), 500

# ============================================================
# GET TRIP GPS HISTORY
# ============================================================

@app.get("/api/trips/<int:trip_id>/gps")
def get_trip_gps_history(trip_id):

    if trip_id <= 0:
        return jsonify({
            "status": "error",
            "reason": "INVALID_TRIP_ID"
        }), 400

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                # ------------------------------------------------
                # FIND TRIP
                # ------------------------------------------------

                cur.execute(
                    """
                    SELECT
                        t.id,
                        t.trip_code,
                        t.vehicle_id,
                        t.origin_name,
                        t.destination_name,
                        t.status,
                        t.started_at,

                        v.vehicle_code,
                        v.device_id,
                        v.status AS vehicle_status

                    FROM trips t

                    JOIN vehicles v
                        ON v.id = t.vehicle_id

                    WHERE t.id = %s

                    LIMIT 1
                    """,
                    (trip_id,)
                )

                trip = cur.fetchone()


                if trip is None:
                    return jsonify({
                        "status": "ok",
                        "found": False,
                        "reason": "TRIP_NOT_FOUND"
                    }), 200


                # ------------------------------------------------
                # GET GPS POINTS
                # ------------------------------------------------

                cur.execute(
                    """
                    SELECT
                        id,
                        latitude,
                        longitude,
                        speed_kmh,
                        satellites,
                        recorded_at

                    FROM gps_logs

                    WHERE trip_id = %s

                    ORDER BY recorded_at ASC, id ASC
                    """,
                    (trip_id,)
                )

                rows = cur.fetchall()


        points = []

        for row in rows:

            points.append({
                "gps_log_id": row["id"],
                "latitude": row["latitude"],
                "longitude": row["longitude"],
                "speed_kmh": row["speed_kmh"],
                "satellites": row["satellites"],
                "recorded_at":
                    row["recorded_at"].isoformat()
                    if row["recorded_at"]
                    else None
            })


        return jsonify({

            "status": "ok",
            "found": True,

            "trip": {
                "id": trip["id"],
                "trip_code": trip["trip_code"],
                "status": trip["status"],
                "origin_name": trip["origin_name"],
                "destination_name": trip["destination_name"],
                "started_at":
                    trip["started_at"].isoformat()
                    if trip["started_at"]
                    else None
            },

            "vehicle": {
                "vehicle_code": trip["vehicle_code"],
                "device_id": trip["device_id"],
                "status": trip["vehicle_status"]
            },

            "point_count": len(points),

            "points": points

        }), 200


    except Exception as e:

        print(
            "[GET TRIP GPS ERROR]",
            repr(e)
        )

        return jsonify({
            "status": "error",
            "reason": "SERVER_ERROR",
            "message": "Could not read trip GPS history"
        }), 500


# ============================================================
# LOCAL RUN
# ============================================================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=8000
    )