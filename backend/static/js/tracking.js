/* ============================================================
   SMARTFLEET - FLEET TRACKING
   tracking.js

   Chức năng:
   - Hiển thị toàn bộ xe đang chạy
   - Một marker cho mỗi xe
   - Tìm kiếm xe / tài xế / chuyến đi
   - Click xe để xem chi tiết
   - Focus riêng một xe
   - Vẽ lịch sử hành trình của trip
   - Auto refresh mỗi 10 giây
============================================================ */


/* ============================================================
   CONFIG
============================================================ */

const TRACKING_API = "/api/tracking/active";
const HEALTH_API = "/health";

const REFRESH_INTERVAL = 10000;


/* ============================================================
   GLOBAL STATE
============================================================ */

let map = null;

let vehicleMarkers = new Map();

let fleetVehicles = [];

let selectedVehicle = null;

let selectedVehicleId = null;

let routePolyline = null;

let refreshTimer = null;

let countdownTimer = null;

let countdownValue = REFRESH_INTERVAL / 1000;

let firstMapLoad = true;

let routeTripId = null;


/* ============================================================
   DOM
============================================================ */

const vehicleListElement =
    document.getElementById("vehicleList");

const vehicleSearchElement =
    document.getElementById("vehicleSearch");

const refreshButton =
    document.getElementById("refreshButton");

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
   INIT
============================================================ */

document.addEventListener("DOMContentLoaded", async () => {

    initializeMap();

    registerEvents();

    await checkServer();

    await loadFleetTracking();

    startAutoRefresh();

});


/* ============================================================
   MAP
============================================================ */

function initializeMap() {

    map = L.map("map", {
        zoomControl: true
    });

    /*
        Không hardcode vị trí xe.

        Đây chỉ là góc nhìn ban đầu.
        Khi API trả GPS, map sẽ tự fit theo marker.
    */

    map.setView([16.0, 108.0], 5);

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

    if (refreshButton) {

        refreshButton.addEventListener(
            "click",
            async () => {

                await manualRefresh();

            }
        );

    }


    if (vehicleSearchElement) {

        vehicleSearchElement.addEventListener(
            "input",
            () => {

                filterVehicleList();

            }
        );

    }


    if (focusVehicleButton) {

        focusVehicleButton.addEventListener(
            "click",
            () => {

                focusSelectedVehicle();

            }
        );

    }


    if (showRouteButton) {

        showRouteButton.addEventListener(
            "click",
            async () => {

                if (!selectedVehicle) {
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
            "Health check error:",
            error
        );

        setServerOnline(false);

    }

}


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
   LOAD ACTIVE FLEET
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
                `Tracking API HTTP ${response.status}`
            );

        }


        const data = await response.json();


        if (data.status !== "ok") {

            throw new Error(
                data.message ||
                "Tracking API returned an error"
            );

        }


        fleetVehicles =
            Array.isArray(data.vehicles)
                ? data.vehicles
                : [];


        updateSummary();

        renderVehicleList(
            fleetVehicles
        );

        updateFleetMarkers(
            fleetVehicles
        );


        /*
            Nếu xe đang được chọn,
            lấy object mới nhất sau refresh.
        */

        if (selectedVehicleId !== null) {

            const updatedVehicle =
                fleetVehicles.find(
                    vehicle =>
                        String(vehicle.vehicle_id) ===
                        String(selectedVehicleId)
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
            "Fleet tracking error:",
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

    const total =
        fleetVehicles.length;


    const gpsOnline =
        fleetVehicles.filter(
            vehicle =>
                vehicle.gps_available === true &&
                isValidGPS(vehicle.gps)
        ).length;


    /*
        Một driver có thể xuất hiện một lần
        trong active trip hiện tại.

        Dùng Set để tránh đếm trùng.
    */

    const driverIds =
        new Set(
            fleetVehicles
                .filter(
                    vehicle =>
                        vehicle.driver_id !== null &&
                        vehicle.driver_id !== undefined
                )
                .map(
                    vehicle =>
                        String(vehicle.driver_id)
                )
        );


    if (activeVehicleCount) {

        activeVehicleCount.textContent =
            total;

    }


    if (gpsOnlineCount) {

        gpsOnlineCount.textContent =
            gpsOnline;

    }


    if (activeDriverCount) {

        activeDriverCount.textContent =
            driverIds.size;

    }


    if (vehicleListCount) {

        vehicleListCount.textContent =
            `${total} phương tiện`;

    }


    if (mapVehicleCount) {

        mapVehicleCount.textContent =
            `${gpsOnline} xe`;

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
                    nào đang thực hiện chuyến đi.
                </p>
            </div>
        `;

        return;

    }


    vehicles.forEach(vehicle => {

        const item =
            createVehicleListItem(
                vehicle
            );

        vehicleListElement.appendChild(
            item
        );

    });

}


/* ============================================================
   CREATE VEHICLE ITEM
============================================================ */

function createVehicleListItem(vehicle) {

    const item =
        document.createElement("div");


    item.className =
        "vehicle-item";


    item.dataset.vehicleId =
        vehicle.vehicle_id;


    if (
        selectedVehicleId !== null &&
        String(selectedVehicleId) ===
        String(vehicle.vehicle_id)
    ) {

        item.classList.add(
            "active"
        );

    }


    const gpsAvailable =
        vehicle.gps_available === true &&
        isValidGPS(vehicle.gps);


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

            <span
                class="vehicle-item-status"
                style="${
                    gpsAvailable
                        ? ""
                        : "color:#94a3b8"
                }"
            >
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
                            ? formatSpeed(
                                vehicle.gps.speed_kmh
                            ) + " km/h"
                            : "--"
                    }
                </strong>
            </div>

        </div>
    `;


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

                const searchableText = [
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


                return searchableText.includes(
                    keyword
                );

            }
        );


    renderVehicleList(
        filtered
    );

}


/* ============================================================
   MARKERS
============================================================ */

function updateFleetMarkers(vehicles) {

    const activeMarkerIds =
        new Set();


    const markerBounds = [];


    vehicles.forEach(vehicle => {

        if (!isValidGPS(vehicle.gps)) {
            return;
        }


        const vehicleId =
            String(vehicle.vehicle_id);


        activeMarkerIds.add(
            vehicleId
        );


        const latitude =
            Number(vehicle.gps.latitude);

        const longitude =
            Number(vehicle.gps.longitude);


        markerBounds.push([
            latitude,
            longitude
        ]);


        let marker =
            vehicleMarkers.get(
                vehicleId
            );


        if (!marker) {

            marker = L.marker([
                latitude,
                longitude
            ]);


            marker.addTo(map);


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

    });


    /*
        Xóa marker của xe không còn
        IN_PROGRESS.
    */

    for (
        const [
            vehicleId,
            marker
        ]
        of vehicleMarkers.entries()
    ) {

        if (
            !activeMarkerIds.has(
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


    /*
        Chỉ fit toàn bộ đội xe ở lần đầu.
        Auto refresh không tự giật map
        về toàn bộ xe.
    */

    if (
        firstMapLoad &&
        markerBounds.length > 0
    ) {

        if (markerBounds.length === 1) {

            map.setView(
                markerBounds[0],
                16
            );

        }
        else {

            map.fitBounds(
                markerBounds,
                {
                    padding: [40, 40]
                }
            );

        }


        firstMapLoad = false;

    }

}


/* ============================================================
   POPUP
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
                Driver:
                <strong>
                    ${escapeHTML(
                        vehicle.driver_code || "--"
                    )}
                </strong>
            </div>


            <div class="vehicle-popup-row">
                Trip:
                <strong>
                    ${escapeHTML(
                        vehicle.trip_code || "--"
                    )}
                </strong>
            </div>


            <div class="vehicle-popup-row">
                Speed:
                <strong>
                    ${formatSpeed(
                        gps.speed_kmh
                    )} km/h
                </strong>
            </div>


            <div class="vehicle-popup-row">
                Satellites:
                <strong>
                    ${
                        gps.satellites ??
                        "--"
                    }
                </strong>
            </div>


            <div class="vehicle-popup-row">
                Updated:
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
                String(item.vehicle_id) ===
                String(vehicleId)
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
        isValidGPS(vehicle.gps)
    ) {

        const latitude =
            Number(
                vehicle.gps.latitude
            );

        const longitude =
            Number(
                vehicle.gps.longitude
            );


        map.flyTo(
            [
                latitude,
                longitude
            ],
            Math.max(
                map.getZoom(),
                16
            ),
            {
                duration: 0.6
            }
        );


        const marker =
            vehicleMarkers.get(
                String(vehicle.vehicle_id)
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
   SELECTED PANEL
============================================================ */

function updateSelectedVehiclePanel(vehicle) {

    const gps =
        vehicle.gps || {};


    if (selectedVehicleStatus) {

        selectedVehicleStatus.textContent =
            translateVehicleStatus(
                vehicle.vehicle_status
            );


        selectedVehicleStatus.classList.add(
            "active"
        );

    }


    setText(
        selectedVehicleCode,
        vehicle.vehicle_code
    );


    setText(
        selectedDeviceId,
        vehicle.device_id
    );


    setText(
        selectedDriverName,
        vehicle.driver_name
    );


    setText(
        selectedDriverCode,
        vehicle.driver_code
    );


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


    setText(
        selectedOrigin,
        vehicle.origin_name
    );


    setText(
        selectedDestination,
        vehicle.destination_name
    );


    if (focusVehicleButton) {

        focusVehicleButton.disabled =
            !isValidGPS(vehicle.gps);

    }


    if (showRouteButton) {

        showRouteButton.disabled =
            !vehicle.trip_id;

    }


    /*
        Camera chưa có backend.
        Giữ disabled.
    */

    if (cameraButton) {

        cameraButton.disabled =
            true;

    }

}


/* ============================================================
   HIGHLIGHT VEHICLE
============================================================ */

function highlightSelectedVehicle() {

    document
        .querySelectorAll(
            ".vehicle-item"
        )
        .forEach(
            item => {

                const isSelected =
                    String(
                        item.dataset.vehicleId
                    ) ===
                    String(
                        selectedVehicleId
                    );


                item.classList.toggle(
                    "active",
                    isSelected
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
        field => setText(
            field,
            "--"
        )
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
   FOCUS VEHICLE
============================================================ */

function focusSelectedVehicle() {

    if (
        !selectedVehicle ||
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

async function showVehicleRoute(
    tripId
) {

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
                `Trip GPS HTTP ${response.status}`
            );

        }


        const data =
            await response.json();


        /*
            Backend hiện tại đã trả GPS history.
            Hỗ trợ một số tên phổ biến để frontend
            không phụ thuộc vào duy nhất một wrapper.
        */

        const points =
            extractGPSPoints(
                data
            );


        if (!points.length) {

            alert(
                "Chuyến đi này chưa có dữ liệu GPS."
            );

            return;

        }


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
                "Không tìm thấy tọa độ GPS hợp lệ."
            );

            return;

        }


        clearRoute();


        routePolyline =
            L.polyline(
                latLngs,
                {
                    weight: 4,
                    opacity: 0.8
                }
            )
            .addTo(map);


        routeTripId =
            tripId;


        map.fitBounds(
            routePolyline.getBounds(),
            {
                padding: [45, 45]
            }
        );

    }
    catch (error) {

        console.error(
            "Load trip route error:",
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


    /*
        API có thể trả trực tiếp array.
    */

    if (Array.isArray(data)) {

        return data;

    }


    /*
        Các wrapper thường dùng.
    */

    if (
        Array.isArray(
            data.points
        )
    ) {

        return data.points;

    }


    if (
        Array.isArray(
            data.gps
        )
    ) {

        return data.gps;

    }


    if (
        Array.isArray(
            data.gps_logs
        )
    ) {

        return data.gps_logs;

    }


    if (
        Array.isArray(
            data.locations
        )
    ) {

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

    routeTripId = null;

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
                    countdownValue <= 0
                ) {

                    countdownValue =
                        REFRESH_INTERVAL /
                        1000;

                }


                if (updateCountdown) {

                    updateCountdown.textContent =
                        countdownValue;

                }

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


    if (updateCountdown) {

        updateCountdown.textContent =
            countdownValue;

    }

}


/* ============================================================
   API STATUS
============================================================ */

function setTrackingStatus(text) {

    if (trackingApiStatus) {

        trackingApiStatus.textContent =
            text;

    }

}


/* ============================================================
   ERROR STATE
============================================================ */

function showVehicleListError() {

    if (!vehicleListElement) {
        return;
    }


    /*
        Nếu đã có dữ liệu cũ,
        giữ nguyên dữ liệu thay vì xóa toàn bộ UI.
    */

    if (fleetVehicles.length > 0) {
        return;
    }


    vehicleListElement.innerHTML = `
        <div class="empty-state">

            <i class="fa-solid fa-triangle-exclamation"></i>

            <p>
                Không thể tải dữ liệu theo dõi đội xe.
                Hãy kiểm tra kết nối tới server.
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
   FORMAT DATE
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
   STATUS TRANSLATION
============================================================ */

function translateVehicleStatus(
    status
) {

    const value =
        String(
            status || ""
        ).toUpperCase();


    switch (value) {

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


function translateTripStatus(
    status
) {

    const value =
        String(
            status || ""
        ).toUpperCase();


    switch (value) {

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
   SAFE TEXT
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