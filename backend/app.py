import os
from datetime import datetime, timezone
import psycopg
from psycopg.rows import dict_row
from werkzeug.security import check_password_hash
from functools import wraps

from flask import (
    Response,
    Flask,
    request,
    jsonify,
    render_template,
    session,
    redirect,
    url_for
)

app = Flask(__name__)

app.secret_key = os.environ.get(
    "FLASK_SECRET_KEY",
    "dev-secret-key-change-me"
)

app.config.update(
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE="Lax"
)

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
        connect_timeout=10,
        row_factory=dict_row
    )
# ============================================================
# ADMIN AUTHENTICATION
# ============================================================

def admin_login_required(view_function):

    @wraps(view_function)
    def wrapped_view(*args, **kwargs):

        if not session.get("admin_user_id"):
            return redirect(url_for("login_page"))

        if session.get("admin_role") != "ADMIN":
            session.clear()
            return redirect(url_for("login_page"))

        return view_function(*args, **kwargs)

    return wrapped_view

# ============================================================
# ADMIN LOGIN API
# ============================================================

@app.post("/api/auth/login")
def admin_login():

    data = request.get_json(silent=True) or {}

    email = str(data.get("email", "")).strip().lower()
    password = str(data.get("password", ""))

    # --------------------------------------------------------
    # 1. VALIDATE REQUEST
    # --------------------------------------------------------

    if not email or not password:
        return jsonify({
            "status": "error",
            "authenticated": False,
            "reason": "MISSING_FIELDS",
            "message": "Email and password are required"
        }), 400

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                # ------------------------------------------------
                # 2. FIND USER
                # ------------------------------------------------

                cur.execute(
                    """
                    SELECT
                        id,
                        full_name,
                        email,
                        password_hash,
                        role,
                        status
                    FROM users
                    WHERE LOWER(email) = %s
                    LIMIT 1
                    """,
                    (email,)
                )

                user = cur.fetchone()

        # --------------------------------------------------------
        # 3. USER NOT FOUND
        # --------------------------------------------------------

        if user is None:
            return jsonify({
                "status": "ok",
                "authenticated": False,
                "reason": "INVALID_CREDENTIALS"
            }), 401

        # --------------------------------------------------------
        # 4. ADMIN ONLY
        # --------------------------------------------------------

        if user["role"] != "ADMIN":
            return jsonify({
                "status": "ok",
                "authenticated": False,
                "reason": "ADMIN_REQUIRED"
            }), 403

        # --------------------------------------------------------
        # 5. ACCOUNT MUST BE ACTIVE
        # --------------------------------------------------------

        if user["status"] != "ACTIVE":
            return jsonify({
                "status": "ok",
                "authenticated": False,
                "reason": "ACCOUNT_NOT_ACTIVE"
            }), 403

        # --------------------------------------------------------
        # 6. VERIFY PASSWORD
        # --------------------------------------------------------

        if not check_password_hash(
            user["password_hash"],
            password
        ):
            return jsonify({
                "status": "ok",
                "authenticated": False,
                "reason": "INVALID_CREDENTIALS"
            }), 401

        # --------------------------------------------------------
        # 7. LOGIN SUCCESS
        # --------------------------------------------------------

# --------------------------------------------------------
# 7. CREATE ADMIN SESSION
# --------------------------------------------------------

        session.clear()

        session["admin_user_id"] = user["id"]
        session["admin_name"] = user["full_name"]
        session["admin_email"] = user["email"]
        session["admin_role"] = user["role"]

        return jsonify({
            "status": "ok",
            "authenticated": True,

            "user": {
                "id": user["id"],
                "full_name": user["full_name"],
                "email": user["email"],
                "role": user["role"]
            }

        }), 200

    except Exception as e:

        print("[ADMIN LOGIN ERROR]", repr(e))

        return jsonify({
            "status": "error",
            "authenticated": False,
            "reason": "SERVER_ERROR",
            "message": "Could not login"
        }), 500
    
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
# WEB - ADMIN LOGIN
# ============================================================

@app.get("/login")
def login_page():

    if session.get("admin_user_id"):
        return redirect(url_for("dashboard"))

    return render_template("login.html")


# ============================================================
# WEB - ADMIN LOGOUT
# ============================================================

@app.post("/logout")
def logout():

    session.clear()

    return jsonify({
        "status": "ok",
        "logged_out": True
    }), 200

# ============================================================
# WEB DASHBOARD
# ============================================================

@app.get("/dashboard")
@admin_login_required
def dashboard():
    return render_template("dashboard.html")


@app.get("/vehicles")
@admin_login_required
def vehicles_page():
    return render_template("vehicles.html")


@app.get("/drivers")
@admin_login_required
def drivers_page():
    return render_template("drivers.html")


@app.get("/trips")
@admin_login_required
def trips_page():
    return render_template("trips.html")


@app.get("/tracking")
@admin_login_required
def tracking_page():
    return render_template("tracking.html")

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
# GET ACTIVE FLEET TRACKING
# Latest GPS position of every active trip
# ============================================================

@app.get("/api/tracking/active")
def get_active_fleet_tracking():

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                cur.execute(
                    """
                    SELECT
                        v.id AS vehicle_id,
                        v.vehicle_code,
                        v.device_id,
                        v.status AS vehicle_status,

                        t.id AS trip_id,
                        t.trip_code,
                        t.status AS trip_status,
                        t.origin_name,
                        t.destination_name,
                        t.started_at,

                        d.id AS driver_id,
                        d.driver_code,
                        u.full_name AS driver_name,

                        gps.id AS gps_log_id,
                        gps.latitude,
                        gps.longitude,
                        gps.speed_kmh,
                        gps.satellites,
                        gps.recorded_at AS last_gps_at

                    FROM trips t

                    JOIN vehicles v
                        ON v.id = t.vehicle_id

                    JOIN driver_profiles d
                        ON d.id = t.driver_id

                    JOIN users u
                        ON u.id = d.user_id

                    LEFT JOIN LATERAL (
                        SELECT
                            g.id,
                            g.latitude,
                            g.longitude,
                            g.speed_kmh,
                            g.satellites,
                            g.recorded_at
                        FROM gps_logs g
                        WHERE g.trip_id = t.id
                          AND g.vehicle_id = v.id
                        ORDER BY g.recorded_at DESC
                        LIMIT 1
                    ) gps ON TRUE

                    WHERE t.status = 'IN_PROGRESS'

                    ORDER BY
                        gps.recorded_at DESC NULLS LAST,
                        v.id ASC
                    """
                )

                rows = cur.fetchall()

        vehicles = []

        for row in rows:

            vehicles.append({
                "vehicle_id": row["vehicle_id"],
                "vehicle_code": row["vehicle_code"],
                "device_id": row["device_id"],
                "vehicle_status": row["vehicle_status"],

                "driver_id": row["driver_id"],
                "driver_code": row["driver_code"],
                "driver_name": row["driver_name"],

                "trip_id": row["trip_id"],
                "trip_code": row["trip_code"],
                "trip_status": row["trip_status"],
                "origin_name": row["origin_name"],
                "destination_name": row["destination_name"],

                "started_at":
                    row["started_at"].isoformat()
                    if row["started_at"]
                    else None,

                "gps": {
                    "gps_log_id": row["gps_log_id"],
                    "latitude": row["latitude"],
                    "longitude": row["longitude"],
                    "speed_kmh": row["speed_kmh"],
                    "satellites": row["satellites"],

                    "last_gps_at":
                        row["last_gps_at"].isoformat()
                        if row["last_gps_at"]
                        else None
                },

                "gps_available":
                    row["gps_log_id"] is not None
            })

        return jsonify({
            "status": "ok",
            "count": len(vehicles),
            "vehicles": vehicles
        }), 200

    except Exception as e:

        print(
            "[ACTIVE FLEET TRACKING ERROR]",
            repr(e)
        )

        return jsonify({
            "status": "error",
            "message": "Could not load active fleet tracking."
        }), 500
    
# ============================================================
# COMPLETE TRIP
# ============================================================

@app.post("/api/trips/<int:trip_id>/complete")
def complete_trip(trip_id):

    try:

        with get_db_connection() as conn:

            with conn.cursor() as cur:

                # ====================================================
                # 1. FIND + LOCK TRIP
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        id,
                        trip_code,
                        driver_id,
                        vehicle_id,
                        status,
                        started_at,
                        completed_at
                    FROM trips
                    WHERE id = %s
                    FOR UPDATE
                    """,
                    (trip_id,)
                )

                trip = cur.fetchone()

                if trip is None:
                    return jsonify({
                        "status": "ok",
                        "completed": False,
                        "reason": "TRIP_NOT_FOUND"
                    }), 200

                # ====================================================
                # 2. IDEMPOTENT - ALREADY COMPLETED
                # ====================================================

                if trip["status"] == "COMPLETED":

                    return jsonify({
                        "status": "ok",
                        "completed": True,
                        "already_completed": True,

                        "trip": {
                            "id": trip["id"],
                            "trip_code": trip["trip_code"],
                            "status": trip["status"],
                            "completed_at":
                                trip["completed_at"].isoformat()
                                if trip["completed_at"]
                                else None
                        }
                    }), 200

                # ====================================================
                # 3. TRIP MUST BE IN_PROGRESS
                # ====================================================

                if trip["status"] != "IN_PROGRESS":

                    return jsonify({
                        "status": "ok",
                        "completed": False,
                        "reason": "INVALID_TRIP_STATUS",
                        "trip_status": trip["status"]
                    }), 200

                # ====================================================
                # 4. FIND + LOCK VEHICLE
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        id,
                        vehicle_code,
                        device_id,
                        status
                    FROM vehicles
                    WHERE id = %s
                    FOR UPDATE
                    """,
                    (trip["vehicle_id"],)
                )

                vehicle = cur.fetchone()

                if vehicle is None:

                    return jsonify({
                        "status": "ok",
                        "completed": False,
                        "reason": "VEHICLE_NOT_FOUND"
                    }), 200

                # ====================================================
                # 5. COMPLETE TRIP
                # ====================================================

                cur.execute(
                    """
                    UPDATE trips
                    SET
                        status = 'COMPLETED',
                        completed_at = CURRENT_TIMESTAMP
                    WHERE id = %s
                    RETURNING
                        id,
                        trip_code,
                        status,
                        started_at,
                        completed_at
                    """,
                    (trip_id,)
                )

                updated_trip = cur.fetchone()

                # ====================================================
                # 6. RELEASE VEHICLE
                # ====================================================

                cur.execute(
                    """
                    UPDATE vehicles
                    SET status = 'AVAILABLE'
                    WHERE id = %s
                    RETURNING
                        id,
                        vehicle_code,
                        device_id,
                        status
                    """,
                    (trip["vehicle_id"],)
                )

                updated_vehicle = cur.fetchone()

                # ====================================================
                # 7. WRITE ACCESS LOG
                # ====================================================

                write_access_log(
                    cur=cur,
                    driver_id=trip["driver_id"],
                    vehicle_id=trip["vehicle_id"],
                    trip_id=trip["id"],
                    rfid_uid=None,
                    event="TRIP_COMPLETED",
                    granted=True,
                    reason="COMPLETED"
                )

                # ====================================================
                # 8. RESPONSE
                # ====================================================

                return jsonify({
                    "status": "ok",
                    "completed": True,
                    "already_completed": False,

                    "trip": {
                        "id": updated_trip["id"],
                        "trip_code": updated_trip["trip_code"],
                        "status": updated_trip["status"],

                        "started_at":
                            updated_trip["started_at"].isoformat()
                            if updated_trip["started_at"]
                            else None,

                        "completed_at":
                            updated_trip["completed_at"].isoformat()
                            if updated_trip["completed_at"]
                            else None
                    },

                    "vehicle": {
                        "id": updated_vehicle["id"],
                        "vehicle_code":
                            updated_vehicle["vehicle_code"],
                        "device_id":
                            updated_vehicle["device_id"],
                        "status":
                            updated_vehicle["status"]
                    }

                }), 200

    except Exception as e:

        print("[COMPLETE TRIP ERROR]", repr(e))

        return jsonify({
            "status": "error",
            "completed": False,
            "reason": "SERVER_ERROR",
            "message": "Could not complete trip"
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
# GET ALL ACCESS LOGS
# ============================================================

@app.get("/api/access-logs")
def get_access_logs():
    try:
        with get_db_connection() as conn:
            with conn.cursor() as cur:

                cur.execute(
                    """
                    SELECT
                        al.id,
                        al.rfid_uid,
                        al.event,
                        al.granted,
                        al.reason,
                        al.created_at,

                        dp.id AS driver_id,
                        dp.driver_code,

                        u.full_name AS driver_name,

                        v.id AS vehicle_id,
                        v.vehicle_code,
                        v.device_id,

                        t.id AS trip_id,
                        t.trip_code

                    FROM access_logs al

                    LEFT JOIN driver_profiles dp
                        ON dp.id = al.driver_id

                    LEFT JOIN users u
                        ON u.id = dp.user_id

                    LEFT JOIN vehicles v
                        ON v.id = al.vehicle_id

                    LEFT JOIN trips t
                        ON t.id = al.trip_id

                    ORDER BY al.created_at DESC
                    """
                )

                rows = cur.fetchall()

        logs = []

        for row in rows:
            logs.append({
                "id": row["id"],

                "driver_id": row["driver_id"],
                "driver_code": row["driver_code"],
                "driver_name": row["driver_name"],

                "vehicle_id": row["vehicle_id"],
                "vehicle_code": row["vehicle_code"],
                "device_id": row["device_id"],

                "trip_id": row["trip_id"],
                "trip_code": row["trip_code"],

                "rfid_uid": row["rfid_uid"],
                "event": row["event"],
                "granted": row["granted"],
                "reason": row["reason"],

                "created_at":
                    row["created_at"].isoformat()
                    if row["created_at"]
                    else None
            })

        return jsonify({
            "status": "ok",
            "count": len(logs),
            "logs": logs
        }), 200

    except Exception as e:

        print("[GET ACCESS LOGS ERROR]", repr(e))

        return jsonify({
            "status": "error",
            "reason": "SERVER_ERROR",
            "message": "Could not read access logs"
        }), 500

# ============================================================
# DASHBOARD SUMMARY
# ============================================================

@app.get("/api/dashboard/summary")
def get_dashboard_summary():
    try:
        with get_db_connection() as conn:
            with conn.cursor() as cur:

                # ====================================================
                # 1. DRIVERS
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        COUNT(dp.id) AS total_drivers,

                        COUNT(dp.id) FILTER (
                            WHERE u.status = 'ACTIVE'
                        ) AS active_drivers

                    FROM driver_profiles dp

                    JOIN users u
                        ON u.id = dp.user_id
                    """
                )

                driver_stats = cur.fetchone()

                # ====================================================
                # 2. VEHICLES
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        COUNT(*) AS total_vehicles,

                        COUNT(*) FILTER (
                            WHERE status = 'AVAILABLE'
                        ) AS available_vehicles,

                        COUNT(*) FILTER (
                            WHERE status = 'IN_TRIP'
                        ) AS vehicles_in_trip,

                        COUNT(*) FILTER (
                            WHERE status = 'MAINTENANCE'
                        ) AS maintenance_vehicles,

                        COUNT(*) FILTER (
                            WHERE status = 'DISABLED'
                        ) AS disabled_vehicles

                    FROM vehicles
                    """
                )

                vehicle_stats = cur.fetchone()

                # ====================================================
                # 3. TRIPS
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        COUNT(*) AS total_trips,

                        COUNT(*) FILTER (
                            WHERE status = 'IN_PROGRESS'
                        ) AS active_trips,

                        COUNT(*) FILTER (
                            WHERE status = 'COMPLETED'
                        ) AS completed_trips,

                        COUNT(*) FILTER (
                            WHERE status = 'CANCELLED'
                        ) AS cancelled_trips

                    FROM trips
                    """
                )

                trip_stats = cur.fetchone()

                # ====================================================
                # 4. GPS
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        COUNT(*) AS total_gps_logs,
                        MAX(recorded_at) AS last_gps_at

                    FROM gps_logs
                    """
                )

                gps_stats = cur.fetchone()

                # ====================================================
                # 5. ACCESS LOGS
                # ====================================================

                cur.execute(
                    """
                    SELECT
                        COUNT(*) AS total_access_logs,

                        COUNT(*) FILTER (
                            WHERE granted = TRUE
                        ) AS granted_access,

                        COUNT(*) FILTER (
                            WHERE granted = FALSE
                        ) AS denied_access

                    FROM access_logs
                    """
                )

                access_stats = cur.fetchone()

        # ============================================================
        # RESPONSE
        # ============================================================

        return jsonify({
            "status": "ok",

            "summary": {

                "drivers": {
                    "total": driver_stats["total_drivers"],
                    "active": driver_stats["active_drivers"]
                },

                "vehicles": {
                    "total": vehicle_stats["total_vehicles"],
                    "available": vehicle_stats["available_vehicles"],
                    "in_trip": vehicle_stats["vehicles_in_trip"],
                    "maintenance": vehicle_stats["maintenance_vehicles"],
                    "disabled": vehicle_stats["disabled_vehicles"]
                },

                "trips": {
                    "total": trip_stats["total_trips"],
                    "active": trip_stats["active_trips"],
                    "completed": trip_stats["completed_trips"],
                    "cancelled": trip_stats["cancelled_trips"]
                },

                "gps": {
                    "total_logs": gps_stats["total_gps_logs"],

                    "last_gps_at":
                        gps_stats["last_gps_at"].isoformat()
                        if gps_stats["last_gps_at"]
                        else None
                },

                "access": {
                    "total": access_stats["total_access_logs"],
                    "granted": access_stats["granted_access"],
                    "denied": access_stats["denied_access"]
                }
            }

        }), 200

    except Exception as e:

        print("[DASHBOARD SUMMARY ERROR]", repr(e))

        return jsonify({
            "status": "error",
            "reason": "SERVER_ERROR",
            "message": "Could not load dashboard summary"
        }), 500
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
# CAMERA CLOUD
# ESP32 -> A7680C -> 4G -> Azure Flask
# ============================================================

CAMERA_DIR = os.path.join("/tmp", "smartfleet_camera")
os.makedirs(CAMERA_DIR, exist_ok=True)
_CAMERA_META = {}

def _safe_camera_device_id(value):
    value = str(value or "").strip()
    if not value:
        return None
    if not all(ch.isalnum() or ch in ("-", "_") for ch in value):
        return None
    return value[:80]

@app.post("/api/camera/upload")
def camera_upload():
    device_id = _safe_camera_device_id(request.args.get("device_id"))
    trip_text = str(request.args.get("trip_id", "")).strip()

    if not device_id:
        return jsonify(status="error", uploaded=False, reason="INVALID_DEVICE_ID"), 400

    try:
        trip_id = int(trip_text)
    except (TypeError, ValueError):
        return jsonify(status="error", uploaded=False, reason="INVALID_TRIP_ID"), 400

    if trip_id <= 0:
        return jsonify(status="error", uploaded=False, reason="INVALID_TRIP_ID"), 400

    if (request.content_type or "").split(";")[0].strip().lower() != "image/jpeg":
        return jsonify(status="error", uploaded=False,
                       reason="CONTENT_TYPE_MUST_BE_IMAGE_JPEG"), 415

    jpeg = request.get_data(cache=False)

    if not jpeg or len(jpeg) > 2 * 1024 * 1024:
        return jsonify(status="error", uploaded=False, reason="INVALID_JPEG_SIZE"), 400

    if len(jpeg) < 4 or jpeg[:2] != b"\xff\xd8" or jpeg[-2:] != b"\xff\xd9":
        return jsonify(status="error", uploaded=False, reason="INVALID_JPEG_DATA"), 400

    path = os.path.join(CAMERA_DIR, f"{device_id}.jpg")
    temp = path + ".tmp"

    with open(temp, "wb") as f:
        f.write(jpeg)
    os.replace(temp, path)

    captured_at = datetime.now(timezone.utc).isoformat()
    _CAMERA_META[device_id] = {
        "trip_id": trip_id,
        "captured_at": captured_at,
        "size_bytes": len(jpeg)
    }

    return jsonify(
        status="ok",
        uploaded=True,
        device_id=device_id,
        trip_id=trip_id,
        size_bytes=len(jpeg),
        captured_at=captured_at,
        image_url=f"/api/camera/latest/{device_id}",
        view_url=f"/camera/{device_id}"
    ), 200

@app.get("/api/camera/latest/<device_id>")
def camera_latest(device_id):
    device_id = _safe_camera_device_id(device_id)
    if not device_id:
        return jsonify(status="error", reason="INVALID_DEVICE_ID"), 400

    path = os.path.join(CAMERA_DIR, f"{device_id}.jpg")
    if not os.path.isfile(path):
        return jsonify(status="ok", found=False, reason="NO_CAMERA_FRAME"), 404

    with open(path, "rb") as f:
        jpeg = f.read()

    return Response(
        jpeg,
        mimetype="image/jpeg",
        headers={
            "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
            "Pragma": "no-cache"
        }
    )

@app.get("/api/camera/status/<device_id>")
def camera_status(device_id):
    device_id = _safe_camera_device_id(device_id)
    if not device_id:
        return jsonify(status="error", reason="INVALID_DEVICE_ID"), 400

    meta = _CAMERA_META.get(device_id)
    path = os.path.join(CAMERA_DIR, f"{device_id}.jpg")

    if meta is None and not os.path.isfile(path):
        return jsonify(status="ok", found=False, device_id=device_id), 200

    if meta is None:
        meta = {
            "trip_id": None,
            "captured_at": None,
            "size_bytes": os.path.getsize(path)
        }

    return jsonify(
        status="ok",
        found=True,
        device_id=device_id,
        **meta,
        image_url=f"/api/camera/latest/{device_id}",
        view_url=f"/camera/{device_id}"
    ), 200

@app.get("/camera/<device_id>")
def camera_page(device_id):
    device_id = _safe_camera_device_id(device_id)
    if not device_id:
        return "Invalid device_id", 400

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartFleet Live Camera - {device_id}</title>
<style>
:root{{
  --bg:#f4f6f8;--panel:#fff;--text:#17202a;--muted:#6b7280;
  --line:#e5e7eb;--dark:#111827;--green:#16a34a;--red:#dc2626;
}}
*{{box-sizing:border-box}}
body{{margin:0;background:var(--bg);color:var(--text);font-family:Inter,Arial,sans-serif}}
.topbar{{height:68px;background:#fff;border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between;padding:0 28px;position:sticky;top:0;z-index:10}}
.brand{{font-size:21px;font-weight:800;letter-spacing:-.4px}} .brand span{{color:#2563eb}}
.device-pill{{display:flex;gap:9px;align-items:center;background:#f8fafc;border:1px solid var(--line);padding:9px 13px;border-radius:999px;font-size:13px;font-weight:700}}
.dot{{width:9px;height:9px;border-radius:50%;background:#9ca3af}} .dot.live{{background:var(--green);box-shadow:0 0 0 4px rgba(22,163,74,.12)}}
.shell{{max-width:1280px;margin:0 auto;padding:28px}}
.header{{display:flex;align-items:flex-end;justify-content:space-between;gap:20px;margin-bottom:20px}}
.eyebrow{{font-size:12px;text-transform:uppercase;letter-spacing:1.3px;color:#64748b;font-weight:800}}
h1{{margin:6px 0 5px;font-size:30px;letter-spacing:-.8px}} .subtitle{{margin:0;color:var(--muted);font-size:14px}}
.grid{{display:grid;grid-template-columns:minmax(0,1.7fr) minmax(280px,.8fr);gap:20px;align-items:start}}
.card{{background:var(--panel);border:1px solid var(--line);border-radius:18px;box-shadow:0 8px 28px rgba(15,23,42,.05);overflow:hidden}}
.card-head{{padding:17px 19px;border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between;gap:12px}}
.card-title{{font-size:15px;font-weight:800}} .live-badge{{display:flex;align-items:center;gap:8px;font-size:12px;font-weight:800;color:var(--green);background:#f0fdf4;border:1px solid #bbf7d0;padding:7px 10px;border-radius:999px}}
.camera-wrap{{position:relative;background:#0b0f14;min-height:420px;display:flex;align-items:center;justify-content:center}}
#cam{{display:none;width:100%;height:auto;max-height:650px;object-fit:contain;image-rendering:auto}}
#empty{{color:#94a3b8;text-align:center;padding:50px 20px}} #empty strong{{display:block;color:#e5e7eb;font-size:18px;margin-bottom:7px}}
.camera-footer{{display:flex;gap:10px;flex-wrap:wrap;padding:14px 18px;background:#fff;border-top:1px solid var(--line)}}
.metric{{font-size:12px;color:#475569;background:#f8fafc;border:1px solid var(--line);border-radius:9px;padding:7px 10px}} .metric b{{color:#111827}}
.side{{display:grid;gap:20px}}
.info{{padding:19px}} .info h2{{font-size:15px;margin:0 0 16px}}
.row{{display:flex;justify-content:space-between;gap:16px;padding:12px 0;border-bottom:1px solid #f0f2f5;font-size:13px}} .row:last-child{{border-bottom:0}}
.label{{color:var(--muted)}} .value{{font-weight:750;text-align:right;word-break:break-word}}
.status-box{{padding:17px 19px;display:flex;gap:12px;align-items:flex-start}}
.status-icon{{width:38px;height:38px;border-radius:11px;background:#f0fdf4;display:grid;place-items:center;color:var(--green);font-weight:900}}
.status-copy strong{{font-size:14px}} .status-copy p{{margin:5px 0 0;color:var(--muted);font-size:12px;line-height:1.5}}
.tech{{margin-top:20px;padding:15px 18px;border:1px dashed #cbd5e1;border-radius:14px;color:#64748b;font-size:12px;background:#f8fafc}}
.tech b{{color:#334155}}
@media(max-width:850px){{.grid{{grid-template-columns:1fr}}.shell{{padding:18px}}.topbar{{padding:0 18px}}.camera-wrap{{min-height:260px}}.header{{align-items:flex-start;flex-direction:column}}}}
</style>
</head>
<body>
<header class="topbar">
  <div class="brand"><span>Smart</span>Fleet</div>
  <div class="device-pill"><span id="topDot" class="dot"></span>{device_id}</div>
</header>
<main class="shell">
  <div class="header">
    <div><div class="eyebrow">Fleet monitoring</div><h1>Live Camera</h1><p class="subtitle">Live vehicle view for active trip monitoring.</p></div>
  </div>
  <section class="grid">
    <article class="card">
      <div class="card-head"><div class="card-title">Vehicle camera</div><div id="liveBadge" class="live-badge" style="color:#64748b;background:#f8fafc;border-color:#e5e7eb"><span id="badgeDot" class="dot"></span><span id="badgeText">WAITING</span></div></div>
      <div class="camera-wrap">
        <div id="empty"><strong>Waiting for camera</strong>Camera will appear automatically when ESP32 uploads a frame.</div>
        <img id="cam" alt="Latest SmartFleet camera frame">
      </div>
      <div class="camera-footer">
        <div class="metric">Status: <b id="mStatus">Waiting</b></div>
        <div class="metric">Updated: <b id="mUpdated">--</b></div>
        <div class="metric">Frame: <b id="mSize">--</b></div>
      </div>
    </article>
    <aside class="side">
      <div class="card info"><h2>Trip information</h2>
        <div class="row"><span class="label">Device</span><span class="value">{device_id}</span></div>
        <div class="row"><span class="label">Trip ID</span><span id="tripId" class="value">--</span></div>
        <div class="row"><span class="label">Camera</span><span class="value">OV3660</span></div>
        <div class="row"><span class="label">Connection</span><span class="value">4G · A7680C</span></div>
        <div class="row"><span class="label">Cloud</span><span class="value">Azure</span></div>
      </div>
      <div class="card status-box"><div class="status-icon">●</div><div class="status-copy"><strong id="connectionTitle">Camera standby</strong><p id="connectionText">Waiting for the first JPEG frame from the vehicle.</p></div></div>
    </aside>
  </section>
  <div class="tech"><b>Data path:</b> OV3660 → ESP32-S3 → A7680C → 4G → Azure → SmartFleet Web. This page shows the latest JPEG frame and refreshes automatically.</div>
</main>
<script>
const DEVICE={device_id!r};
const cam=document.getElementById('cam'), empty=document.getElementById('empty');
const topDot=document.getElementById('topDot'), badgeDot=document.getElementById('badgeDot');
const badge=document.getElementById('liveBadge'), badgeText=document.getElementById('badgeText');
let lastCaptured=null;
function ageText(iso){{
 if(!iso) return 'available';
 const ms=Date.now()-new Date(iso).getTime();
 if(!Number.isFinite(ms)) return 'available';
 const s=Math.max(0,Math.floor(ms/1000));
 if(s<2) return 'just now'; if(s<60) return s+'s ago'; return Math.floor(s/60)+'m ago';
}}
function setLive(live){{
 topDot.classList.toggle('live',live); badgeDot.classList.toggle('live',live);
 badgeText.textContent=live?'LIVE':'WAITING';
 badge.style.color=live?'#16a34a':'#64748b'; badge.style.background=live?'#f0fdf4':'#f8fafc'; badge.style.borderColor=live?'#bbf7d0':'#e5e7eb';
 document.getElementById('mStatus').textContent=live?'Online':'Waiting';
 document.getElementById('connectionTitle').textContent=live?'Camera online':'Camera standby';
 document.getElementById('connectionText').textContent=live?'Receiving camera frames from '+DEVICE+'.':'Waiting for the first JPEG frame from the vehicle.';
}}
async function refresh(){{
 const t=Date.now();
 try{{
   const r=await fetch('/api/camera/status/'+encodeURIComponent(DEVICE)+'?t='+t,{{cache:'no-store'}});
   const d=await r.json();
   if(d.found){{
     const age=d.captured_at?Date.now()-new Date(d.captured_at).getTime():0;
     const live=!d.captured_at || age<15000;
     setLive(live);
     document.getElementById('tripId').textContent=d.trip_id??'--';
     document.getElementById('mSize').textContent=d.size_bytes?Math.round(d.size_bytes/1024*10)/10+' KB':'--';
     document.getElementById('mUpdated').textContent=ageText(d.captured_at);
     if(d.captured_at!==lastCaptured){{lastCaptured=d.captured_at;cam.src='/api/camera/latest/'+encodeURIComponent(DEVICE)+'?t='+t;}}
     cam.style.display='block'; empty.style.display='none';
   }}else{{setLive(false);cam.style.display='none';empty.style.display='block';}}
 }}catch(e){{setLive(false);document.getElementById('mStatus').textContent='Unavailable';}}
}}
cam.onerror=()=>{{cam.style.display='none';empty.style.display='block';}};
cam.onload=()=>{{cam.style.display='block';empty.style.display='none';}};
refresh(); setInterval(refresh,1000);
</script>
</body>
</html>"""


# ============================================================
# LOCAL RUN
# ============================================================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=8000
    )