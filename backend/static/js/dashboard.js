/* ============================================================
   SMARTFLEET DASHBOARD
   dashboard.js
   ============================================================ */


/* ============================================================
   CONFIGURATION
   ============================================================ */

const DEVICE_ID = "TRUCK_001";

const REFRESH_INTERVAL = 10000; // 10 seconds


/* ============================================================
   GLOBAL VARIABLES
   ============================================================ */

let map = null;

let vehicleMarker = null;

let routePolyline = null;

let currentTripId = null;

let firstMapLoad = true;

let countdown = 10;


/* ============================================================
   INITIALIZE MAP
   ============================================================ */

function initializeMap() {

    /*
     * Không hard-code vị trí xe thật.
     * Bản đồ ban đầu chỉ hiển thị ở mức tổng quát.
     * Khi API trả GPS, map sẽ tự di chuyển đến xe.
     */

    map = L.map("map").setView(
        [10.8, 106.7],
        11
    );


    /*
     * OpenStreetMap
     */

    L.tileLayer(
        "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
        {
            maxZoom: 19,

            attribution:
                '&copy; OpenStreetMap contributors'
        }
    ).addTo(map);
}


/* ============================================================
   API STATUS
   ============================================================ */

function setConnectionStatus(online) {

    const dot =
        document.getElementById("connectionDot");

    const text =
        document.getElementById("connectionText");

    const apiStatus =
        document.getElementById("apiStatus");


    if (online) {

        dot.classList.remove("dot-error");

        dot.classList.add("dot-online");


        text.textContent =
            "Đã kết nối";


        apiStatus.textContent =
            "Online";

        apiStatus.className =
            "status-online";

    } else {

        dot.classList.remove("dot-online");

        dot.classList.add("dot-error");


        text.textContent =
            "Mất kết nối";


        apiStatus.textContent =
            "Offline";

        apiStatus.className =
            "status-error";
    }
}


/* ============================================================
   SAFE VALUE
   ============================================================ */

function displayValue(value, fallback = "--") {

    if (
        value === null ||
        value === undefined ||
        value === ""
    ) {
        return fallback;
    }

    return value;
}


/* ============================================================
   FORMAT DATE
   ============================================================ */

function formatDateTime(dateString) {

    if (!dateString) {
        return "--";
    }


    const date =
        new Date(dateString);


    if (Number.isNaN(date.getTime())) {
        return "--";
    }


    return date.toLocaleString(
        "vi-VN",
        {
            hour12: false
        }
    );
}


/* ============================================================
   UPDATE VEHICLE INFORMATION
   ============================================================ */

function updateVehicleInformation(data) {

    if (!data) {
        return;
    }


    /* --------------------------------------------------------
       VEHICLE
       -------------------------------------------------------- */

    if (data.vehicle) {

        document.getElementById(
            "vehicleCode"
        ).textContent =
            displayValue(
                data.vehicle.vehicle_code
            );


        document.getElementById(
            "deviceId"
        ).textContent =
            displayValue(
                data.vehicle.device_id
            );


        document.getElementById(
            "vehicleStatus"
        ).textContent =
            "Vehicle: " +
            displayValue(
                data.vehicle.status
            );
    }


    /* --------------------------------------------------------
       TRIP
       -------------------------------------------------------- */

    if (data.trip) {

        currentTripId =
            data.trip.id;


        document.getElementById(
            "tripCode"
        ).textContent =
            displayValue(
                data.trip.trip_code
            );


        document.getElementById(
            "tripId"
        ).textContent =
            "Trip ID: " +
            displayValue(
                data.trip.id
            );


        document.getElementById(
            "tripStatus"
        ).textContent =
            displayValue(
                data.trip.status
            );


        document.getElementById(
            "originName"
        ).textContent =
            displayValue(
                data.trip.origin_name
            );


        document.getElementById(
            "destinationName"
        ).textContent =
            displayValue(
                data.trip.destination_name
            );


        document.getElementById(
            "routeDescription"
        ).textContent =
            displayValue(
                data.trip.origin_name,
                "Điểm xuất phát"
            )
            +
            " → "
            +
            displayValue(
                data.trip.destination_name,
                "Điểm đến"
            );
    }


    /* --------------------------------------------------------
       LOCATION
       -------------------------------------------------------- */

    if (data.location) {

        const location =
            data.location;


        document.getElementById(
            "speed"
        ).textContent =
            displayValue(
                location.speed_kmh
            );


        document.getElementById(
            "satellites"
        ).textContent =
            displayValue(
                location.satellites
            );


        document.getElementById(
            "gpsLogId"
        ).textContent =
            displayValue(
                location.gps_log_id
            );


        document.getElementById(
            "lastUpdate"
        ).textContent =
            formatDateTime(
                location.recorded_at
            );


        updateVehicleMarker(
            location.latitude,
            location.longitude,
            location.speed_kmh
        );
    }
}


/* ============================================================
   UPDATE VEHICLE MARKER
   ============================================================ */

function updateVehicleMarker(
    latitude,
    longitude,
    speed
) {

    const lat =
        Number(latitude);

    const lng =
        Number(longitude);


    if (
        !Number.isFinite(lat) ||
        !Number.isFinite(lng)
    ) {

        console.warn(
            "Invalid GPS coordinates"
        );

        return;
    }


    const position =
        [lat, lng];


    /* --------------------------------------------------------
       CREATE MARKER
       -------------------------------------------------------- */

    if (!vehicleMarker) {

        vehicleMarker =
            L.marker(position)
                .addTo(map);


        vehicleMarker.bindPopup(
            "<strong>TRUCK_001</strong>"
            +
            "<br>Tốc độ: "
            +
            displayValue(speed, 0)
            +
            " km/h"
        );

    } else {

        vehicleMarker.setLatLng(
            position
        );


        vehicleMarker.setPopupContent(
            "<strong>TRUCK_001</strong>"
            +
            "<br>Tốc độ: "
            +
            displayValue(speed, 0)
            +
            " km/h"
        );
    }


    /* --------------------------------------------------------
       FIRST LOAD
       -------------------------------------------------------- */

    if (firstMapLoad) {

        map.setView(
            position,
            16
        );


        vehicleMarker.openPopup();


        firstMapLoad = false;
    }
}


/* ============================================================
   LOAD LATEST VEHICLE LOCATION
   ============================================================ */

async function loadLatestLocation() {

    const url =
        `/api/vehicles/${encodeURIComponent(DEVICE_ID)}/location`;


    try {

        const response =
            await fetch(
                url,
                {
                    method: "GET",

                    headers: {
                        "Accept":
                            "application/json"
                    },

                    cache: "no-store"
                }
            );


        if (!response.ok) {

            throw new Error(
                `HTTP ${response.status}`
            );
        }


        const data =
            await response.json();


        if (
            data.status !== "ok"
        ) {

            throw new Error(
                data.reason ||
                "API_ERROR"
            );
        }


        setConnectionStatus(true);


        /*
         * API có thể trả:
         *
         * found = false
         *
         * khi chưa có GPS.
         */

        if (
            data.found === false
        ) {

            document.getElementById(
                "routeDescription"
            ).textContent =
                "Chưa có dữ liệu GPS";

            return;
        }


        updateVehicleInformation(
            data
        );


        /*
         * Khi đã biết Trip ID,
         * lấy toàn bộ lịch sử GPS.
         */

        if (currentTripId) {

            await loadTripHistory(
                currentTripId
            );
        }

    } catch (error) {

        console.error(
            "Latest location error:",
            error
        );


        setConnectionStatus(false);
    }
}


/* ============================================================
   LOAD TRIP GPS HISTORY
   ============================================================ */

async function loadTripHistory(
    tripId
) {

    const url =
        `/api/trips/${encodeURIComponent(tripId)}/gps`;


    try {

        const response =
            await fetch(
                url,
                {
                    method: "GET",

                    headers: {
                        "Accept":
                            "application/json"
                    },

                    cache: "no-store"
                }
            );


        if (!response.ok) {

            throw new Error(
                `HTTP ${response.status}`
            );
        }


        const data =
            await response.json();


        if (
            data.status !== "ok" ||
            data.found === false
        ) {

            return;
        }


        const points =
            Array.isArray(data.points)
                ? data.points
                : [];


        document.getElementById(
            "gpsPointCount"
        ).textContent =
            data.point_count ??
            points.length;


        drawRoute(points);

    } catch (error) {

        console.error(
            "Trip history error:",
            error
        );
    }
}


/* ============================================================
   DRAW GPS ROUTE
   ============================================================ */

function drawRoute(points) {

    if (
        !Array.isArray(points) ||
        points.length === 0
    ) {

        return;
    }


    const coordinates = [];


    for (const point of points) {

        const lat =
            Number(point.latitude);

        const lng =
            Number(point.longitude);


        if (
            Number.isFinite(lat) &&
            Number.isFinite(lng)
        ) {

            coordinates.push(
                [lat, lng]
            );
        }
    }


    if (
        coordinates.length === 0
    ) {

        return;
    }


    /* --------------------------------------------------------
       CREATE ROUTE
       -------------------------------------------------------- */

    if (!routePolyline) {

        routePolyline =
            L.polyline(
                coordinates,
                {
                    weight: 5,
                    opacity: 0.8
                }
            ).addTo(map);

    } else {

        routePolyline.setLatLngs(
            coordinates
        );
    }
}


/* ============================================================
   MANUAL REFRESH BUTTON
   ============================================================ */

async function manualRefresh() {

    const button =
        document.getElementById(
            "refreshButton"
        );


    button.disabled = true;

    button.textContent =
        "Đang tải...";


    await loadLatestLocation();


    countdown = 10;


    button.disabled = false;

    button.textContent =
        "Cập nhật";
}


/* ============================================================
   COUNTDOWN
   ============================================================ */

function updateCountdown() {

    document.getElementById(
        "updateCountdown"
    ).textContent =
        countdown + "s";


    countdown--;


    if (countdown < 0) {

        countdown = 10;
    }
}


/* ============================================================
   AUTO REFRESH
   ============================================================ */

function startAutoRefresh() {

    setInterval(
        () => {

            countdown = 10;

            loadLatestLocation();

        },
        REFRESH_INTERVAL
    );


    setInterval(
        updateCountdown,
        1000
    );
}


/* ============================================================
   START DASHBOARD
   ============================================================ */

document.addEventListener(
    "DOMContentLoaded",
    async () => {

        console.log(
            "SmartFleet Dashboard starting..."
        );


        /* Initialize Leaflet */

        initializeMap();


        /* Refresh button */

        document.getElementById(
            "refreshButton"
        ).addEventListener(
            "click",
            manualRefresh
        );


        /* First API request */

        await loadLatestLocation();


        /* Start automatic refresh */

        startAutoRefresh();


        console.log(
            "SmartFleet Dashboard ready"
        );
    }
);