/* ============================================================
   SMARTFLEET DASHBOARD
   dashboard.js
   ============================================================ */


/* ============================================================
   CONFIGURATION
   ============================================================ */

const DEVICE_ID = "TRUCK_001";

const REFRESH_INTERVAL = 10000;


/* ============================================================
   GLOBAL VARIABLES
   ============================================================ */

let map = null;

let vehicleMarker = null;

let routePolyline = null;

let currentTripId = null;

let firstMapLoad = true;

let countdown = 10;

let isRefreshing = false;


/* ============================================================
   SAFE DOM HELPERS
   ============================================================ */

function getElement(id) {

    return document.getElementById(id);

}


function setText(id, value) {

    const element = getElement(id);

    if (!element) {
        return;
    }

    element.textContent = value;

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


    const date = new Date(dateString);


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
   INITIALIZE MAP
   ============================================================ */

function initializeMap() {

    const mapElement = getElement("map");


    if (!mapElement) {

        console.warn(
            "Map element not found."
        );

        return;

    }


    /*
        Không hard-code vị trí thật của xe.

        Bản đồ chỉ mở ở mức tổng quát.
        Khi API trả GPS hợp lệ, map sẽ tự di chuyển đến xe.
    */

    map = L.map("map").setView(
        [10.8, 106.7],
        11
    );


    L.tileLayer(
        "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
        {
            maxZoom: 19,

            attribution:
                "&copy; OpenStreetMap contributors"
        }
    ).addTo(map);

}


/* ============================================================
   CONNECTION STATUS
   ============================================================ */

function setConnectionStatus(online) {

    const dot =
        getElement("connectionDot");

    const text =
        getElement("connectionText");

    const apiStatus =
        getElement("apiStatus");

    const systemApiStatus =
        getElement("systemApiStatus");


    if (online) {

        if (dot) {

            dot.classList.remove(
                "dot-error"
            );

            dot.classList.add(
                "dot-online"
            );

        }


        if (text) {

            text.textContent =
                "Đã kết nối";

        }


        if (apiStatus) {

            apiStatus.textContent =
                "Online";

            apiStatus.className =
                "status-online";

        }


        if (systemApiStatus) {

            systemApiStatus.textContent =
                "Online";

            systemApiStatus.className =
                "status-online";

        }

    } else {

        if (dot) {

            dot.classList.remove(
                "dot-online"
            );

            dot.classList.add(
                "dot-error"
            );

        }


        if (text) {

            text.textContent =
                "Mất kết nối";

        }


        if (apiStatus) {

            apiStatus.textContent =
                "Offline";

            apiStatus.className =
                "status-error";

        }


        if (systemApiStatus) {

            systemApiStatus.textContent =
                "Offline";

            systemApiStatus.className =
                "status-error";

        }

    }

}


/* ============================================================
   FETCH JSON
   ============================================================ */

async function fetchJson(url) {

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
            `${url} returned HTTP ${response.status}`
        );

    }


    return await response.json();

}


/* ============================================================
   LOAD DASHBOARD SUMMARY
   ============================================================ */

async function loadDashboardSummary() {

    try {

        const data =
            await fetchJson(
                "/api/dashboard/summary"
            );


        if (
            data.status !== "ok" ||
            !data.summary
        ) {

            throw new Error(
                "Invalid dashboard summary"
            );

        }


        const summary =
            data.summary;


        /* ----------------------------------------------------
           VEHICLES
           ---------------------------------------------------- */

        if (summary.vehicles) {

            setText(
                "totalVehicles",
                displayValue(
                    summary.vehicles.total,
                    0
                )
            );


            setText(
                "availableVehicles",
                displayValue(
                    summary.vehicles.available,
                    0
                )
            );


            setText(
                "vehiclesInTrip",
                displayValue(
                    summary.vehicles.in_trip,
                    0
                )
            );

        }


        /* ----------------------------------------------------
           DRIVERS
           ---------------------------------------------------- */

        if (summary.drivers) {

            setText(
                "activeDrivers",
                displayValue(
                    summary.drivers.active,
                    0
                )
            );


            setText(
                "totalDrivers",
                displayValue(
                    summary.drivers.total,
                    0
                )
            );

        }


        /* ----------------------------------------------------
           TRIPS
           ---------------------------------------------------- */

        if (summary.trips) {

            setText(
                "completedTrips",
                displayValue(
                    summary.trips.completed,
                    0
                )
            );


            setText(
                "totalTrips",
                displayValue(
                    summary.trips.total,
                    0
                )
            );


            setText(
                "activeTrips",
                displayValue(
                    summary.trips.active,
                    0
                )
            );

        }


        /* ----------------------------------------------------
           GPS
           ---------------------------------------------------- */

        if (summary.gps) {

            /*
                Đây là tổng GPS logs toàn hệ thống.

                Khi có trip đang hoạt động,
                loadTripHistory() có thể cập nhật lại
                gpsPointCount thành số điểm của trip đó.
            */

            if (!currentTripId) {

                setText(
                    "gpsPointCount",
                    displayValue(
                        summary.gps.total_logs,
                        0
                    )
                );

            }

        }


        /* ----------------------------------------------------
           ACCESS
           ---------------------------------------------------- */

        if (summary.access) {

            setText(
                "accessGranted",
                displayValue(
                    summary.access.granted,
                    0
                )
            );


            setText(
                "accessDenied",
                displayValue(
                    summary.access.denied,
                    0
                )
            );

        }


        setConnectionStatus(true);


        console.log(
            "Dashboard summary loaded:",
            summary
        );

    } catch (error) {

        console.error(
            "Dashboard summary error:",
            error
        );

    }

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

        setText(
            "vehicleCode",
            displayValue(
                data.vehicle.vehicle_code
            )
        );


        setText(
            "deviceId",
            displayValue(
                data.vehicle.device_id,
                DEVICE_ID
            )
        );


        setText(
            "vehicleStatus",
            "Vehicle: " +
            displayValue(
                data.vehicle.status
            )
        );

    }


    /* --------------------------------------------------------
       TRIP
       -------------------------------------------------------- */

    if (data.trip) {

        currentTripId =
            data.trip.id;


        setText(
            "tripCode",
            displayValue(
                data.trip.trip_code
            )
        );


        setText(
            "tripId",
            "Trip ID: " +
            displayValue(
                data.trip.id
            )
        );


        setText(
            "tripStatus",
            displayValue(
                data.trip.status
            )
        );


        setText(
            "originName",
            displayValue(
                data.trip.origin_name
            )
        );


        setText(
            "destinationName",
            displayValue(
                data.trip.destination_name
            )
        );


        setText(
            "routeDescription",

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
            )
        );

    } else {

        /*
            Xe có thể không có chuyến đang hoạt động.

            Đây là trạng thái bình thường sau khi
            trip đã COMPLETED.
        */

        currentTripId = null;


        setText(
            "tripCode",
            "Không có chuyến"
        );


        setText(
            "tripId",
            "Trip ID: --"
        );


        setText(
            "tripStatus",
            "IDLE"
        );


        setText(
            "originName",
            "--"
        );


        setText(
            "destinationName",
            "--"
        );


        setText(
            "routeDescription",
            "Phương tiện hiện không có chuyến đang hoạt động"
        );

    }


    /* --------------------------------------------------------
       LOCATION
       -------------------------------------------------------- */

    if (data.location) {

        const location =
            data.location;


        setText(
            "speed",
            displayValue(
                location.speed_kmh,
                0
            )
        );


        setText(
            "satellites",
            displayValue(
                location.satellites
            )
        );


        setText(
            "gpsLogId",
            displayValue(
                location.gps_log_id
            )
        );


        setText(
            "lastUpdate",
            formatDateTime(
                location.recorded_at
            )
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

    if (!map) {
        return;
    }


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

            "<strong>" +
            DEVICE_ID +
            "</strong>" +

            "<br>Tốc độ: " +

            displayValue(
                speed,
                0
            )

            +

            " km/h"

        );

    } else {

        vehicleMarker.setLatLng(
            position
        );


        vehicleMarker.setPopupContent(

            "<strong>" +
            DEVICE_ID +
            "</strong>" +

            "<br>Tốc độ: " +

            displayValue(
                speed,
                0
            )

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

        const data =
            await fetchJson(url);


        if (data.status !== "ok") {

            throw new Error(
                data.reason ||
                "LOCATION_API_ERROR"
            );

        }


        setConnectionStatus(true);


        /*
            Nếu chưa từng có GPS.
        */

        if (data.found === false) {

            setText(
                "routeDescription",
                "Chưa có dữ liệu GPS"
            );

            return;

        }


        updateVehicleInformation(
            data
        );


        /*
            Nếu vehicle đang có trip,
            lấy toàn bộ lịch sử GPS của trip.
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

        /*
            Không đánh dấu toàn bộ dashboard offline
            chỉ vì endpoint GPS lỗi.

            Summary/health có thể vẫn đang hoạt động.
        */

    }

}


/* ============================================================
   LOAD TRIP GPS HISTORY
   ============================================================ */

async function loadTripHistory(
    tripId
) {

    if (!tripId) {
        return;
    }


    const url =
        `/api/trips/${encodeURIComponent(tripId)}/gps`;


    try {

        const data =
            await fetchJson(url);


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


        setText(
            "gpsPointCount",

            data.point_count ??
            points.length
        );


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

    if (!map) {
        return;
    }


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


    if (coordinates.length === 0) {
        return;
    }


    /* --------------------------------------------------------
       CREATE / UPDATE ROUTE
       -------------------------------------------------------- */

    if (!routePolyline) {

        routePolyline =
            L.polyline(
                coordinates,
                {
                    weight: 5,
                    opacity: 0.8
                }
            )
            .addTo(map);

    } else {

        routePolyline.setLatLngs(
            coordinates
        );

    }

}


/* ============================================================
   CLEAR ROUTE
   ============================================================ */

function clearRoute() {

    if (
        map &&
        routePolyline
    ) {

        map.removeLayer(
            routePolyline
        );

        routePolyline = null;

    }

}


/* ============================================================
   LOAD ACCESS LOGS
   ============================================================ */

async function loadRecentActivity() {

    const container =
        getElement("recentActivity");


    if (!container) {
        return;
    }


    try {

        const data =
            await fetchJson(
                "/api/access-logs"
            );


        /*
            Backend hiện tại trả:
            {
                status: "ok",
                count: ...,
                logs: [...]
            }

            Có fallback để tránh giao diện hỏng
            nếu sau này đổi key thành access_logs.
        */

        const logs =
            Array.isArray(data.logs)
                ? data.logs
                : Array.isArray(
                    data.access_logs
                )
                    ? data.access_logs
                    : [];


        if (logs.length === 0) {

            container.innerHTML = `
                <div class="activity-empty">
                    <i class="fa-solid fa-clock-rotate-left"></i>
                    <p>Chưa có hoạt động</p>
                </div>
            `;

            return;

        }


        container.innerHTML = "";


        /*
            Dashboard chỉ hiển thị 6 sự kiện mới nhất.
        */

        const recentLogs =
            logs.slice(0, 6);


        for (const log of recentLogs) {

            const item =
                document.createElement(
                    "div"
                );


            item.className =
                "activity-item";


            const granted =
                log.granted === true;


            let iconClass;

            let iconColor;


            if (granted) {

                iconClass =
                    "fa-solid fa-circle-check";

                iconColor =
                    "status-online";

            } else {

                iconClass =
                    "fa-solid fa-circle-xmark";

                iconColor =
                    "status-error";

            }


            const eventName =
                displayValue(
                    log.event,
                    "SYSTEM_EVENT"
                );


            const driverName =
                displayValue(
                    log.driver_name,
                    "Không xác định"
                );


            const vehicleCode =
                displayValue(
                    log.vehicle_code,
                    DEVICE_ID
                );


            const reason =
                displayValue(
                    log.reason,
                    granted
                        ? "GRANTED"
                        : "DENIED"
                );


            item.innerHTML = `

                <div class="activity-icon ${iconColor}">
                    <i class="${iconClass}"></i>
                </div>

                <div class="activity-content">

                    <strong>
                        ${escapeHtml(eventName)}
                    </strong>

                    <span>
                        ${escapeHtml(driverName)}
                        •
                        ${escapeHtml(vehicleCode)}
                        •
                        ${escapeHtml(reason)}
                    </span>

                </div>

                <span class="activity-time">
                    ${escapeHtml(
                        formatDateTime(
                            log.created_at
                        )
                    )}
                </span>

            `;


            container.appendChild(
                item
            );

        }

    } catch (error) {

        console.error(
            "Access logs error:",
            error
        );


        container.innerHTML = `

            <div class="activity-empty">

                <i class="fa-solid fa-triangle-exclamation"></i>

                <p>
                    Không thể tải hoạt động
                </p>

            </div>

        `;

    }

}


/* ============================================================
   ESCAPE HTML
   ============================================================ */

function escapeHtml(value) {

    return String(value)

        .replaceAll(
            "&",
            "&amp;"
        )

        .replaceAll(
            "<",
            "&lt;"
        )

        .replaceAll(
            ">",
            "&gt;"
        )

        .replaceAll(
            '"',
            "&quot;"
        )

        .replaceAll(
            "'",
            "&#039;"
        );

}


/* ============================================================
   HEALTH CHECK
   ============================================================ */

async function checkApiHealth() {

    try {

        const data =
            await fetchJson(
                "/health"
            );


        if (data.status === "ok") {

            setConnectionStatus(true);

            return true;

        }


        throw new Error(
            "Health check failed"
        );

    } catch (error) {

        console.error(
            "Health check error:",
            error
        );


        setConnectionStatus(false);


        return false;

    }

}


/* ============================================================
   LOAD ALL DASHBOARD DATA
   ============================================================ */

async function loadDashboardData() {

    /*
        Promise.allSettled() được dùng thay vì Promise.all().

        Nếu một API lỗi, các API còn lại vẫn tiếp tục cập nhật.
    */

    const results =
        await Promise.allSettled([

            loadDashboardSummary(),

            loadLatestLocation(),

            loadRecentActivity()

        ]);


    for (const result of results) {

        if (
            result.status ===
            "rejected"
        ) {

            console.error(
                "Dashboard request failed:",
                result.reason
            );

        }

    }

}


/* ============================================================
   MANUAL REFRESH
   ============================================================ */

async function manualRefresh() {

    if (isRefreshing) {
        return;
    }


    const button =
        getElement(
            "refreshButton"
        );


    isRefreshing = true;


    if (button) {

        button.disabled = true;

        button.innerHTML = `
            <i class="fa-solid fa-rotate fa-spin"></i>
            <span>Đang tải...</span>
        `;

    }


    await loadDashboardData();


    countdown = 10;


    if (button) {

        button.disabled = false;

        button.innerHTML = `
            <i class="fa-solid fa-rotate"></i>
            <span>Cập nhật</span>
        `;

    }


    isRefreshing = false;

}


/* ============================================================
   COUNTDOWN
   ============================================================ */

function updateCountdown() {

    setText(
        "updateCountdown",
        countdown + "s"
    );


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
        async () => {

            if (isRefreshing) {
                return;
            }


            isRefreshing = true;

            countdown = 10;


            try {

                await loadDashboardData();

            } finally {

                isRefreshing = false;

            }

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


        /* ----------------------------------------------------
           INITIALIZE MAP
           ---------------------------------------------------- */

        initializeMap();


        /* ----------------------------------------------------
           REFRESH BUTTON
           ---------------------------------------------------- */

        const refreshButton =
            getElement(
                "refreshButton"
            );


        if (refreshButton) {

            refreshButton.addEventListener(
                "click",
                manualRefresh
            );

        }


        /* ----------------------------------------------------
           HEALTH CHECK
           ---------------------------------------------------- */

        await checkApiHealth();


        /* ----------------------------------------------------
           FIRST DATA LOAD
           ---------------------------------------------------- */

        await loadDashboardData();


        /* ----------------------------------------------------
           AUTO REFRESH
           ---------------------------------------------------- */

        startAutoRefresh();


        console.log(
            "SmartFleet Dashboard ready"
        );

    }
);