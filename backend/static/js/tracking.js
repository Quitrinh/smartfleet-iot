/* ============================================================
   SMARTFLEET
   FLEET TRACKING
   tracking.js
============================================================ */


/* ============================================================
   CONFIG
============================================================ */

const TRACKING_API = "/api/tracking/active";
const HEALTH_API = "/health";

const REFRESH_INTERVAL = 10000;


/* ============================================================
   STATE
============================================================ */

let map = null;

let fleetVehicles = [];

let vehicleMarkers = new Map();

let selectedVehicle = null;
let selectedVehicleId = null;

let routePolyline = null;

let firstMapLoad = true;

let refreshTimer = null;
let countdownTimer = null;

let countdownValue = REFRESH_INTERVAL / 1000;


/* ============================================================
   DOM
============================================================ */

const vehicleListElement =
    document.getElementById("vehicleList");

const vehicleSearchElement =
    document.getElementById("vehicleSearch");

const refreshButton =
    document.getElementById("refreshButton");


/* Summary */

const activeVehicleCount =
    document.getElementById("activeVehicleCount");

const gpsOnlineCount =
    document.getElementById("gpsOnlineCount");

const activeDriverCount =
    document.getElementById("activeDriverCount");

const updateCountdown =
    document.getElementById("updateCountdown");

const vehicleListCount =
    document.getElementById("vehicleListCount");

const mapVehicleCount =
    document.getElementById("mapVehicleCount");


/* Server */

const serverStatusDot =
    document.getElementById("serverStatusDot");

const serverStatusText =
    document.getElementById("serverStatusText");

const trackingApiStatus =
    document.getElementById("trackingApiStatus");


/* Selected vehicle */

const selectedVehicleStatus =
    document.getElementById("selectedVehicleStatus");

const selectedVehicleCode =
    document.getElementById("selectedVehicleCode");

const selectedDeviceId =
    document.getElementById("selectedDeviceId");

const selectedDriverName =
    document.getElementById("selectedDriverName");

const selectedDriverCode =
    document.getElementById("selectedDriverCode");

const selectedTripCode =
    document.getElementById("selectedTripCode");

const selectedTripStatus =
    document.getElementById("selectedTripStatus");

const selectedSpeed =
    document.getElementById("selectedSpeed");

const selectedSatellites =
    document.getElementById("selectedSatellites");

const selectedLastUpdate =
    document.getElementById("selectedLastUpdate");

const selectedGpsStatus =
    document.getElementById("selectedGpsStatus");

const selectedOrigin =
    document.getElementById("selectedOrigin");

const selectedDestination =
    document.getElementById("selectedDestination");


/* Buttons */

const focusVehicleButton =
    document.getElementById("focusVehicleButton");

const showRouteButton =
    document.getElementById("showRouteButton");

const cameraButton =
    document.getElementById("cameraButton");


/* ============================================================
   START
============================================================ */

document.addEventListener(
    "DOMContentLoaded",
    async () => {

        initMap();

        registerEvents();

        await checkServer();

        await loadFleetTracking();

        startAutoRefresh();

    }
);


/* ============================================================
   MAP
============================================================ */

function initMap() {

    map = L.map(
        "map",
        {
            zoomControl: true
        }
    );


    /*
        Đây chỉ là vị trí nhìn mặc định.

        Khi có GPS thật, bản đồ sẽ tự động
        zoom tới vị trí của đội xe.
    */

    map.setView(
        [16.0, 108.0],
        5
    );


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
   EVENTS
============================================================ */

function registerEvents() {

    /* Refresh */

    if (refreshButton) {

        refreshButton.addEventListener(
            "click",
            async () => {

                await manualRefresh();

            }
        );

    }


    /* Search */

    if (vehicleSearchElement) {

        vehicleSearchElement.addEventListener(
            "input",
            () => {

                filterVehicleList();

            }
        );

    }


    /* Focus vehicle */

    if (focusVehicleButton) {

        focusVehicleButton.addEventListener(
            "click",
            () => {

                focusSelectedVehicle();

            }
        );

    }


    /* Show route */

    if (showRouteButton) {

        showRouteButton.addEventListener(
            "click",
            async () => {

                if (!selectedVehicle) {
                    return;
                }


                if (!selectedVehicle.trip_id) {
                    return;
                }


                await showVehicleRoute(
                    selectedVehicle.trip_id
                );

            }
        );

    }

}


/* ============================================================
   SERVER HEALTH
============================================================ */

async function checkServer() {

    try {

        const response = await fetch(
            HEALTH_API,
            {
                cache: "no-store"
            }
        );


        if (!response.ok) {

            throw new Error(
                `HTTP ${response.status}`
            );

        }


        setServerOnline(true);

    }
    catch (error) {

        console.error(
            "[HEALTH ERROR]",
            error
        );


        setServerOnline(false);

    }

}


/* ============================================================
   SERVER STATUS UI
============================================================ */

function setServerOnline(isOnline) {

    if (serverStatusDot) {

        serverStatusDot.classList.remove(
            "online",
            "offline"
        );


        serverStatusDot.classList.add(
            isOnline
                ? "online"
                : "offline"
        );

    }


    if (serverStatusText) {

        serverStatusText.textContent =
            isOnline
                ? "Server Connected"
                : "Server Offline";

    }

}


/* ============================================================
   LOAD FLEET TRACKING
============================================================ */

async function loadFleetTracking() {

    try {

        setTrackingStatus(
            "Đang cập nhật..."
        );


        const response = await fetch(
            TRACKING_API,
            {
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


        if (data.status !== "ok") {

            throw new Error(
                data.message ||
                "Tracking API error"
            );

        }


        fleetVehicles =
            Array.isArray(data.vehicles)
                ? data.vehicles
                : [];


        /* -----------------------------------------
           Update UI
        ----------------------------------------- */

        updateSummary();

        renderVehicleList(
            fleetVehicles
        );

        updateFleetMarkers(
            fleetVehicles
        );


        /* -----------------------------------------
           AUTO SELECT FIRST VEHICLE

           Nếu chưa chọn xe và có ít nhất 1 xe,
           tự chọn xe đầu tiên.
        ----------------------------------------- */

        if (
            selectedVehicleId === null &&
            fleetVehicles.length > 0
        ) {

            selectVehicle(
                fleetVehicles[0].vehicle_id,
                false
            );

        }


        /* -----------------------------------------
           REFRESH SELECTED VEHICLE

           Nếu đã chọn xe thì lấy object mới nhất
           sau mỗi lần API refresh.
        ----------------------------------------- */

        if (selectedVehicleId !== null) {

            const updatedVehicle =
                fleetVehicles.find(
                    vehicle =>
                        String(
                            vehicle.vehicle_id
                        ) ===
                        String(
                            selectedVehicleId
                        )
                );


            if (updatedVehicle) {

                selectedVehicle =
                    updatedVehicle;


                updateSelectedVehiclePanel(
                    updatedVehicle
                );

            }
            else {

                clearSelectedVehicle();

            }

        }


        setTrackingStatus(
            "Online"
        );


        setServerOnline(true);

    }
    catch (error) {

        console.error(
            "[TRACKING ERROR]",
            error
        );


        setTrackingStatus(
            "Lỗi kết nối"
        );


        showVehicleListError();

    }

}


/* ============================================================
   SUMMARY
============================================================ */

function updateSummary() {

    const totalVehicles =
        fleetVehicles.length;


    const onlineVehicles =
        fleetVehicles.filter(
            vehicle =>
                vehicle.gps_available === true &&
                isValidGPS(vehicle.gps)
        );


    const drivers =
        new Set();


    fleetVehicles.forEach(
        vehicle => {

            if (
                vehicle.driver_id !== null &&
                vehicle.driver_id !== undefined
            ) {

                drivers.add(
                    String(
                        vehicle.driver_id
                    )
                );

            }

        }
    );


    setText(
        activeVehicleCount,
        totalVehicles
    );


    setText(
        gpsOnlineCount,
        onlineVehicles.length
    );


    setText(
        activeDriverCount,
        drivers.size
    );


    if (vehicleListCount) {

        vehicleListCount.textContent =
            `${totalVehicles} phương tiện`;

    }


    if (mapVehicleCount) {

        mapVehicleCount.textContent =
            `${onlineVehicles.length} xe`;

    }

}


/* ============================================================
   VEHICLE LIST
============================================================ */

function renderVehicleList(vehicles) {

    if (!vehicleListElement) {
        return;
    }


    vehicleListElement.innerHTML = "";


    if (!vehicles.length) {

        vehicleListElement.innerHTML = `
            <div class="empty-state">

                <i class="fa-solid fa-truck"></i>

                <p>
                    Hiện không có phương tiện
                    đang thực hiện chuyến đi.
                </p>

            </div>
        `;

        return;

    }


    vehicles.forEach(
        vehicle => {

            const item =
                createVehicleItem(
                    vehicle
                );


            vehicleListElement.appendChild(
                item
            );

        }
    );


    highlightSelectedVehicle();

}


/* ============================================================
   CREATE VEHICLE ITEM
============================================================ */

function createVehicleItem(vehicle) {

    const item =
        document.createElement("div");


    item.className =
        "vehicle-item";


    item.dataset.vehicleId =
        vehicle.vehicle_id;


    const gpsAvailable =
        vehicle.gps_available === true &&
        isValidGPS(
            vehicle.gps
        );


    const gpsText =
        gpsAvailable
            ? "GPS Online"
            : "Chưa có GPS";


    item.innerHTML = `

        <div class="vehicle-item-header">

            <div class="vehicle-item-title">

                <i class="fa-solid fa-truck"></i>

                <span>
                    ${escapeHTML(
                        vehicle.vehicle_code || "--"
                    )}
                </span>

            </div>


            <span class="vehicle-item-status">

                ${gpsText}

            </span>

        </div>


        <div class="vehicle-item-body">

            <div class="vehicle-item-row">

                <span>Tài xế</span>

                <strong>
                    ${escapeHTML(
                        vehicle.driver_code || "--"
                    )}
                </strong>

            </div>


            <div class="vehicle-item-row">

                <span>Chuyến</span>

                <strong>
                    ${escapeHTML(
                        vehicle.trip_code || "--"
                    )}
                </strong>

            </div>


            <div class="vehicle-item-row">

                <span>Thiết bị</span>

                <strong>
                    ${escapeHTML(
                        vehicle.device_id || "--"
                    )}
                </strong>

            </div>


            <div class="vehicle-item-row">

                <span>Tốc độ</span>

                <strong>

                    ${
                        gpsAvailable
                            ? `${formatSpeed(
                                vehicle.gps.speed_kmh
                            )} km/h`
                            : "--"
                    }

                </strong>

            </div>

        </div>
    `;


    if (!gpsAvailable) {

        const status =
            item.querySelector(
                ".vehicle-item-status"
            );


        if (status) {

            status.style.color =
                "#94a3b8";

        }

    }


    item.addEventListener(
        "click",
        () => {

            selectVehicle(
                vehicle.vehicle_id,
                true
            );

        }
    );


    return item;

}


/* ============================================================
   SEARCH
============================================================ */

function filterVehicleList() {

    const keyword =
        (
            vehicleSearchElement?.value ||
            ""
        )
        .trim()
        .toLowerCase();


    if (!keyword) {

        renderVehicleList(
            fleetVehicles
        );

        return;

    }


    const filtered =
        fleetVehicles.filter(
            vehicle => {

                const text = [

                    vehicle.vehicle_code,
                    vehicle.device_id,

                    vehicle.driver_code,
                    vehicle.driver_name,

                    vehicle.trip_code,

                    vehicle.origin_name,
                    vehicle.destination_name

                ]
                .filter(Boolean)
                .join(" ")
                .toLowerCase();


                return text.includes(
                    keyword
                );

            }
        );


    renderVehicleList(
        filtered
    );

}


/* ============================================================
   UPDATE MAP MARKERS
============================================================ */

function updateFleetMarkers(vehicles) {

    const currentMarkerIds =
        new Set();


    const bounds = [];


    vehicles.forEach(
        vehicle => {

            if (
                !isValidGPS(
                    vehicle.gps
                )
            ) {

                return;

            }


            const vehicleId =
                String(
                    vehicle.vehicle_id
                );


            currentMarkerIds.add(
                vehicleId
            );


            const latitude =
                Number(
                    vehicle.gps.latitude
                );


            const longitude =
                Number(
                    vehicle.gps.longitude
                );


            bounds.push([
                latitude,
                longitude
            ]);


            let marker =
                vehicleMarkers.get(
                    vehicleId
                );


            /* New marker */

            if (!marker) {

                marker =
                    L.marker([
                        latitude,
                        longitude
                    ]);


                marker.addTo(
                    map
                );


                marker.on(
                    "click",
                    () => {

                        selectVehicle(
                            vehicle.vehicle_id,
                            false
                        );

                    }
                );


                vehicleMarkers.set(
                    vehicleId,
                    marker
                );

            }

            /* Existing marker */

            else {

                marker.setLatLng([
                    latitude,
                    longitude
                ]);

            }


            marker.bindPopup(
                createVehiclePopup(
                    vehicle
                )
            );

        }
    );


    /* -----------------------------------------
       REMOVE OLD MARKERS
    ----------------------------------------- */

    for (
        const [
            vehicleId,
            marker
        ]
        of vehicleMarkers.entries()
    ) {

        if (
            !currentMarkerIds.has(
                vehicleId
            )
        ) {

            map.removeLayer(
                marker
            );


            vehicleMarkers.delete(
                vehicleId
            );

        }

    }


    /* -----------------------------------------
       FIRST LOAD → SHOW ALL VEHICLES
    ----------------------------------------- */

    if (
        firstMapLoad &&
        bounds.length > 0
    ) {

        if (bounds.length === 1) {

            map.setView(
                bounds[0],
                16
            );

        }
        else {

            map.fitBounds(
                bounds,
                {
                    padding: [
                        40,
                        40
                    ]
                }
            );

        }


        firstMapLoad = false;

    }

}


/* ============================================================
   VEHICLE POPUP
============================================================ */

function createVehiclePopup(vehicle) {

    const gps =
        vehicle.gps || {};


    return `

        <div class="vehicle-popup">

            <div class="vehicle-popup-title">

                ${escapeHTML(
                    vehicle.vehicle_code || "--"
                )}

            </div>


            <div class="vehicle-popup-row">

                Tài xế:

                <strong>
                    ${escapeHTML(
                        vehicle.driver_code || "--"
                    )}
                </strong>

            </div>


            <div class="vehicle-popup-row">

                Chuyến:

                <strong>
                    ${escapeHTML(
                        vehicle.trip_code || "--"
                    )}
                </strong>

            </div>


            <div class="vehicle-popup-row">

                Tốc độ:

                <strong>
                    ${formatSpeed(
                        gps.speed_kmh
                    )} km/h
                </strong>

            </div>


            <div class="vehicle-popup-row">

                Vệ tinh:

                <strong>
                    ${
                        gps.satellites ??
                        "--"
                    }
                </strong>

            </div>


            <div class="vehicle-popup-row">

                Cập nhật:

                <strong>
                    ${formatDateTime(
                        gps.last_gps_at
                    )}
                </strong>

            </div>

        </div>
    `;

}


/* ============================================================
   SELECT VEHICLE
============================================================ */

function selectVehicle(
    vehicleId,
    openPopup = true
) {

    const vehicle =
        fleetVehicles.find(
            item =>
                String(
                    item.vehicle_id
                ) ===
                String(
                    vehicleId
                )
        );


    if (!vehicle) {
        return;
    }


    selectedVehicle =
        vehicle;


    selectedVehicleId =
        vehicle.vehicle_id;


    updateSelectedVehiclePanel(
        vehicle
    );


    highlightSelectedVehicle();


    if (
        isValidGPS(
            vehicle.gps
        )
    ) {

        const latitude =
            Number(
                vehicle.gps.latitude
            );


        const longitude =
            Number(
                vehicle.gps.longitude
            );


        /*
            Auto select lúc load:
            không ép zoom lại vì map đã fit fleet.

            Click thủ công:
            focus vào xe.
        */

        if (openPopup) {

            map.flyTo(
                [
                    latitude,
                    longitude
                ],
                17,
                {
                    duration: 0.6
                }
            );

        }


        const marker =
            vehicleMarkers.get(
                String(
                    vehicle.vehicle_id
                )
            );


        if (
            marker &&
            openPopup
        ) {

            marker.openPopup();

        }

    }

}


/* ============================================================
   SELECTED VEHICLE PANEL
============================================================ */

function updateSelectedVehiclePanel(vehicle) {

    const gps =
        vehicle.gps || {};


    /* Vehicle status */

    if (selectedVehicleStatus) {

        selectedVehicleStatus.textContent =
            translateVehicleStatus(
                vehicle.vehicle_status
            );


        selectedVehicleStatus.classList.add(
            "active"
        );

    }


    /* Vehicle */

    setText(
        selectedVehicleCode,
        vehicle.vehicle_code
    );


    setText(
        selectedDeviceId,
        vehicle.device_id
    );


    /* Driver */

    setText(
        selectedDriverName,
        vehicle.driver_name
    );


    setText(
        selectedDriverCode,
        vehicle.driver_code
    );


    /* Trip */

    setText(
        selectedTripCode,
        vehicle.trip_code
    );


    setText(
        selectedTripStatus,
        translateTripStatus(
            vehicle.trip_status
        )
    );


    /* GPS */

    setText(
        selectedSpeed,
        formatSpeed(
            gps.speed_kmh
        )
    );


    setText(
        selectedSatellites,
        gps.satellites ?? "--"
    );


    setText(
        selectedLastUpdate,
        formatDateTime(
            gps.last_gps_at
        )
    );


    setText(
        selectedGpsStatus,
        vehicle.gps_available
            ? "GPS Online"
            : "Chưa có GPS"
    );


    /* Route */

    setText(
        selectedOrigin,
        vehicle.origin_name
    );


    setText(
        selectedDestination,
        vehicle.destination_name
    );


    /* Buttons */

    if (focusVehicleButton) {

        focusVehicleButton.disabled =
            !isValidGPS(
                vehicle.gps
            );

    }


    if (showRouteButton) {

        showRouteButton.disabled =
            !vehicle.trip_id;

    }


    /*
        Camera backend chưa làm.
    */

    if (cameraButton) {

        cameraButton.disabled =
            true;

    }

}


/* ============================================================
   HIGHLIGHT SELECTED
============================================================ */

function highlightSelectedVehicle() {

    const items =
        document.querySelectorAll(
            ".vehicle-item"
        );


    items.forEach(
        item => {

            const selected =
                String(
                    item.dataset.vehicleId
                ) ===
                String(
                    selectedVehicleId
                );


            item.classList.toggle(
                "active",
                selected
            );

        }
    );

}


/* ============================================================
   CLEAR SELECTED VEHICLE
============================================================ */

function clearSelectedVehicle() {

    selectedVehicle = null;
    selectedVehicleId = null;


    if (selectedVehicleStatus) {

        selectedVehicleStatus.textContent =
            "Chưa chọn xe";


        selectedVehicleStatus.classList.remove(
            "active"
        );

    }


    const fields = [

        selectedVehicleCode,
        selectedDeviceId,

        selectedDriverName,
        selectedDriverCode,

        selectedTripCode,
        selectedTripStatus,

        selectedSpeed,
        selectedSatellites,

        selectedLastUpdate,

        selectedOrigin,
        selectedDestination

    ];


    fields.forEach(
        field => {

            setText(
                field,
                "--"
            );

        }
    );


    setText(
        selectedGpsStatus,
        "Chưa có dữ liệu"
    );


    if (focusVehicleButton) {

        focusVehicleButton.disabled =
            true;

    }


    if (showRouteButton) {

        showRouteButton.disabled =
            true;

    }


    clearRoute();

}


/* ============================================================
   FOCUS SELECTED VEHICLE
============================================================ */

function focusSelectedVehicle() {

    if (!selectedVehicle) {
        return;
    }


    if (
        !isValidGPS(
            selectedVehicle.gps
        )
    ) {

        return;

    }


    const latitude =
        Number(
            selectedVehicle.gps.latitude
        );


    const longitude =
        Number(
            selectedVehicle.gps.longitude
        );


    map.flyTo(
        [
            latitude,
            longitude
        ],
        17,
        {
            duration: 0.7
        }
    );


    const marker =
        vehicleMarkers.get(
            String(
                selectedVehicle.vehicle_id
            )
        );


    if (marker) {

        marker.openPopup();

    }

}


/* ============================================================
   SHOW TRIP ROUTE
============================================================ */

async function showVehicleRoute(tripId) {

    if (!tripId) {
        return;
    }


    try {

        if (showRouteButton) {

            showRouteButton.disabled =
                true;


            showRouteButton.innerHTML = `
                <i class="fa-solid fa-spinner fa-spin"></i>
                Đang tải...
            `;

        }


        const response = await fetch(
            `/api/trips/${encodeURIComponent(
                tripId
            )}/gps`,
            {
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


        const points =
            extractGPSPoints(
                data
            );


        const latLngs =
            points
                .filter(
                    point =>
                        isValidCoordinate(
                            point.latitude,
                            point.longitude
                        )
                )
                .map(
                    point => [
                        Number(
                            point.latitude
                        ),
                        Number(
                            point.longitude
                        )
                    ]
                );


        if (!latLngs.length) {

            alert(
                "Chuyến đi chưa có dữ liệu GPS hợp lệ."
            );

            return;

        }


        clearRoute();


        routePolyline =
            L.polyline(
                latLngs,
                {
                    weight: 4,
                    opacity: 0.85
                }
            );


        routePolyline.addTo(
            map
        );


        map.fitBounds(
            routePolyline.getBounds(),
            {
                padding: [
                    40,
                    40
                ]
            }
        );

    }
    catch (error) {

        console.error(
            "[ROUTE ERROR]",
            error
        );


        alert(
            "Không thể tải hành trình GPS."
        );

    }
    finally {

        if (showRouteButton) {

            showRouteButton.disabled =
                !selectedVehicle?.trip_id;


            showRouteButton.innerHTML = `
                <i class="fa-solid fa-route"></i>
                Xem hành trình
            `;

        }

    }

}


/* ============================================================
   EXTRACT GPS POINTS
============================================================ */

function extractGPSPoints(data) {

    if (!data) {
        return [];
    }


    if (Array.isArray(data)) {
        return data;
    }


    if (Array.isArray(data.points)) {
        return data.points;
    }


    if (Array.isArray(data.gps)) {
        return data.gps;
    }


    if (Array.isArray(data.gps_logs)) {
        return data.gps_logs;
    }


    if (Array.isArray(data.locations)) {
        return data.locations;
    }


    return [];

}


/* ============================================================
   CLEAR ROUTE
============================================================ */

function clearRoute() {

    if (
        routePolyline &&
        map
    ) {

        map.removeLayer(
            routePolyline
        );

    }


    routePolyline = null;

}


/* ============================================================
   MANUAL REFRESH
============================================================ */

async function manualRefresh() {

    if (refreshButton) {

        refreshButton.disabled =
            true;


        refreshButton.innerHTML = `
            <i class="fa-solid fa-spinner fa-spin"></i>
            Đang tải
        `;

    }


    try {

        await Promise.allSettled([
            checkServer(),
            loadFleetTracking()
        ]);


        resetCountdown();

    }
    finally {

        if (refreshButton) {

            refreshButton.disabled =
                false;


            refreshButton.innerHTML = `
                <i class="fa-solid fa-rotate"></i>
                Làm mới
            `;

        }

    }

}


/* ============================================================
   AUTO REFRESH
============================================================ */

function startAutoRefresh() {

    stopAutoRefresh();


    resetCountdown();


    refreshTimer =
        setInterval(
            async () => {

                await loadFleetTracking();

                resetCountdown();

            },
            REFRESH_INTERVAL
        );


    countdownTimer =
        setInterval(
            () => {

                countdownValue--;


                if (
                    countdownValue < 0
                ) {

                    countdownValue =
                        REFRESH_INTERVAL /
                        1000;

                }


                setText(
                    updateCountdown,
                    countdownValue
                );

            },
            1000
        );

}


/* ============================================================
   STOP AUTO REFRESH
============================================================ */

function stopAutoRefresh() {

    if (refreshTimer) {

        clearInterval(
            refreshTimer
        );


        refreshTimer = null;

    }


    if (countdownTimer) {

        clearInterval(
            countdownTimer
        );


        countdownTimer = null;

    }

}


/* ============================================================
   RESET COUNTDOWN
============================================================ */

function resetCountdown() {

    countdownValue =
        REFRESH_INTERVAL /
        1000;


    setText(
        updateCountdown,
        countdownValue
    );

}


/* ============================================================
   TRACKING API STATUS
============================================================ */

function setTrackingStatus(value) {

    setText(
        trackingApiStatus,
        value
    );

}


/* ============================================================
   ERROR UI
============================================================ */

function showVehicleListError() {

    if (!vehicleListElement) {
        return;
    }


    /*
        Nếu đang có dữ liệu cũ thì giữ lại.
    */

    if (fleetVehicles.length > 0) {
        return;
    }


    vehicleListElement.innerHTML = `

        <div class="empty-state">

            <i class="fa-solid fa-triangle-exclamation"></i>

            <p>
                Không thể tải dữ liệu theo dõi đội xe.
            </p>

        </div>
    `;

}


/* ============================================================
   GPS VALIDATION
============================================================ */

function isValidGPS(gps) {

    if (!gps) {
        return false;
    }


    return isValidCoordinate(
        gps.latitude,
        gps.longitude
    );

}


function isValidCoordinate(
    latitude,
    longitude
) {

    const lat =
        Number(latitude);


    const lng =
        Number(longitude);


    if (
        !Number.isFinite(lat) ||
        !Number.isFinite(lng)
    ) {

        return false;

    }


    if (
        lat < -90 ||
        lat > 90
    ) {

        return false;

    }


    if (
        lng < -180 ||
        lng > 180
    ) {

        return false;

    }


    return true;

}


/* ============================================================
   FORMAT SPEED
============================================================ */

function formatSpeed(value) {

    const speed =
        Number(value);


    if (
        !Number.isFinite(speed)
    ) {

        return "--";

    }


    return speed.toFixed(1);

}


/* ============================================================
   FORMAT DATE TIME
============================================================ */

function formatDateTime(value) {

    if (!value) {
        return "--";
    }


    const date =
        new Date(value);


    if (
        Number.isNaN(
            date.getTime()
        )
    ) {

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
   VEHICLE STATUS
============================================================ */

function translateVehicleStatus(status) {

    switch (
        String(
            status || ""
        ).toUpperCase()
    ) {

        case "AVAILABLE":
            return "Sẵn sàng";

        case "IN_TRIP":
            return "Đang trong chuyến";

        case "MAINTENANCE":
            return "Bảo trì";

        case "DISABLED":
            return "Ngừng hoạt động";

        default:
            return status || "--";

    }

}


/* ============================================================
   TRIP STATUS
============================================================ */

function translateTripStatus(status) {

    switch (
        String(
            status || ""
        ).toUpperCase()
    ) {

        case "ASSIGNED":
            return "Đã phân công";

        case "READY":
            return "Sẵn sàng";

        case "IN_PROGRESS":
            return "Đang thực hiện";

        case "ARRIVED":
            return "Đã đến";

        case "DELIVERED":
            return "Đã giao";

        case "COMPLETED":
            return "Hoàn thành";

        case "CANCELLED":
            return "Đã hủy";

        default:
            return status || "--";

    }

}


/* ============================================================
   SET TEXT
============================================================ */

function setText(
    element,
    value
) {

    if (!element) {
        return;
    }


    if (
        value === null ||
        value === undefined ||
        value === ""
    ) {

        element.textContent =
            "--";

        return;

    }


    element.textContent =
        String(value);

}


/* ============================================================
   ESCAPE HTML
============================================================ */

function escapeHTML(value) {

    if (
        value === null ||
        value === undefined
    ) {

        return "--";

    }


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
   CLEANUP
============================================================ */

window.addEventListener(
    "beforeunload",
    () => {

        stopAutoRefresh();

    }
);