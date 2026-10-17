import os
from flask import Flask, request, jsonify
from datetime import datetime, timezone

import psycopg
from psycopg.rows import dict_row


app = Flask(__name__)


# ============================================================
# DATABASE
# ============================================================

def get_db_connection():
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
                        current_user AS db_user,
                        version() AS version
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

                cur.execute("SELECT COUNT(*) AS count FROM driver_profiles")
                drivers = cur.fetchone()["count"]

                cur.execute("SELECT COUNT(*) AS count FROM rfid_cards")
                rfid_cards = cur.fetchone()["count"]

                cur.execute("SELECT COUNT(*) AS count FROM vehicles")
                vehicles = cur.fetchone()["count"]

                cur.execute("SELECT COUNT(*) AS count FROM trips")
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
# ACCESS - CURRENT TEST ENDPOINT
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
# GPS - CURRENT TEST ENDPOINT
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