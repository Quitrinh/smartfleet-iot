from flask import Flask, request, jsonify
from datetime import datetime, timezone

app = Flask(__name__)


@app.get("/")
def home():
    return jsonify({
        "project": "SmartFleet IoT",
        "service": "SmartFleet API",
        "status": "running"
    })


@app.get("/health")
def health():
    return jsonify({
        "status": "ok",
        "service": "smartfleet-api"
    })


@app.post("/api/access")
def access():
    data = request.get_json(silent=True) or {}

    return jsonify({
        "status": "ok",
        "message": "Access event received",
        "received": data,
        "timestamp": datetime.now(timezone.utc).isoformat()
    })


@app.post("/api/gps")
def gps():
    data = request.get_json(silent=True) or {}

    return jsonify({
        "status": "ok",
        "message": "GPS data received",
        "received": data,
        "timestamp": datetime.now(timezone.utc).isoformat()
    })


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)