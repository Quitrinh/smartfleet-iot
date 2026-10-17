/* ============================================================
   SMARTFLEET - VEHICLES PAGE
   vehicles.js
   ============================================================ */

document.addEventListener("DOMContentLoaded", () => {

    /* ========================================================
       CONFIG
       ======================================================== */

    const VEHICLES_API = "/api/vehicles";


    /* ========================================================
       ELEMENTS
       ======================================================== */

    const vehicleTableBody =
        document.getElementById("vehicleTableBody");

    const searchInput =
        document.getElementById("searchInput");

    const refreshButton =
        document.getElementById("refreshButton");

    const totalVehicles =
        document.getElementById("totalVehicles");

    const activeVehicles =
        document.getElementById("activeVehicles");

    const inTripVehicles =
        document.getElementById("inTripVehicles");

    const inactiveVehicles =
        document.getElementById("inactiveVehicles");

    const tableDescription =
        document.getElementById("tableDescription");

    const emptyState =
        document.getElementById("emptyState");

    const serverStatus =
        document.getElementById("serverStatus");

    const statusDot =
        document.getElementById("statusDot");

    const lastUpdate =
        document.getElementById("lastUpdate");


    /* ========================================================
       STATE
       ======================================================== */

    let vehicles = [];


    /* ========================================================
       LOAD VEHICLES
       ======================================================== */

    async function loadVehicles() {

        setLoading(true);

        setServerStatus(
            "loading",
            "Đang kết nối SmartFleet Server..."
        );


        try {

            const response =
                await fetch(
                    VEHICLES_API,
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


            /*
             * Hỗ trợ một số dạng response:
             *
             * 1.
             * {
             *     "vehicles": [...]
             * }
             *
             * 2.
             * {
             *     "data": [...]
             * }
             *
             * 3.
             * [...]
             */

            if (Array.isArray(data)) {

                vehicles = data;

            } else if (
                Array.isArray(data.vehicles)
            ) {

                vehicles = data.vehicles;

            } else if (
                Array.isArray(data.data)
            ) {

                vehicles = data.data;

            } else {

                vehicles = [];
            }


            updateSummary(vehicles);

            renderVehicles(vehicles);


            setServerStatus(
                "online",
                "SmartFleet Server đã kết nối"
            );


            updateLastUpdate();


        } catch (error) {

            console.error(
                "Load vehicles error:",
                error
            );


            vehicles = [];


            updateSummary([]);


            showLoadError();


            setServerStatus(
                "offline",
                "Không thể kết nối SmartFleet Server"
            );

        } finally {

            setLoading(false);
        }
    }


    /* ========================================================
       RENDER VEHICLES
       ======================================================== */

    function renderVehicles(vehicleList) {

        vehicleTableBody.innerHTML = "";


        if (
            !vehicleList ||
            vehicleList.length === 0
        ) {

            emptyState.classList.add("show");

            tableDescription.textContent =
                "Không có phương tiện để hiển thị.";

            return;
        }


        emptyState.classList.remove("show");


        tableDescription.textContent =
            `${vehicleList.length} phương tiện`;


        vehicleList.forEach((vehicle) => {

            const row =
                document.createElement("tr");


            /* ------------------------------------------------
               VEHICLE DATA
               ------------------------------------------------ */

            const vehicleId =
                vehicle.id ?? "--";


            const vehicleCode =
                vehicle.vehicle_code ??
                vehicle.code ??
                vehicle.name ??
                `Vehicle ${vehicleId}`;


            const deviceId =
                vehicle.device_id ??
                "--";


            const status =
                normalizeStatus(
                    vehicle.status
                );


            const trip =
                getTripInformation(
                    vehicle
                );


            const gpsAvailable =
                hasGPS(vehicle);


            /* ------------------------------------------------
               ROW
               ------------------------------------------------ */

            row.innerHTML = `

                <td>

                    <div class="vehicle-name">

                        <strong>
                            ${escapeHTML(vehicleCode)}
                        </strong>

                        <span>
                            ID: ${escapeHTML(vehicleId)}
                        </span>

                    </div>

                </td>


                <td>
                    ${escapeHTML(deviceId)}
                </td>


                <td>

                    ${createStatusBadge(status)}

                </td>


                <td>

                    ${
                        trip
                            ? escapeHTML(trip)
                            : "--"
                    }

                </td>


                <td>

                    <span class="${
                        gpsAvailable
                            ? "gps-online"
                            : "gps-offline"
                    }">

                        ${
                            gpsAvailable
                                ? "Có dữ liệu"
                                : "Chưa có dữ liệu"
                        }

                    </span>

                </td>


                <td>

                    <a
                        class="view-button"
                        href="${createVehicleLink(deviceId)}"
                    >
                        Xem GPS
                    </a>

                </td>
            `;


            vehicleTableBody.appendChild(
                row
            );
        });
    }


    /* ========================================================
       SUMMARY
       ======================================================== */

    function updateSummary(vehicleList) {

        const total =
            vehicleList.length;


        let active = 0;

        let inTrip = 0;

        let inactive = 0;


        vehicleList.forEach((vehicle) => {

            const status =
                normalizeStatus(
                    vehicle.status
                );


            if (
                status === "IN_TRIP"
            ) {

                inTrip++;

            } else if (
                status === "ACTIVE" ||
                status === "AVAILABLE"
            ) {

                active++;

            } else {

                inactive++;
            }
        });


        totalVehicles.textContent =
            total;

        activeVehicles.textContent =
            active;

        inTripVehicles.textContent =
            inTrip;

        inactiveVehicles.textContent =
            inactive;
    }


    /* ========================================================
       SEARCH
       ======================================================== */

    function filterVehicles() {

        const keyword =
            searchInput.value
                .trim()
                .toLowerCase();


        if (!keyword) {

            renderVehicles(vehicles);

            return;
        }


        const filtered =
            vehicles.filter((vehicle) => {

                const searchable =
                    [
                        vehicle.id,
                        vehicle.vehicle_code,
                        vehicle.code,
                        vehicle.name,
                        vehicle.device_id,
                        vehicle.status,
                        vehicle.license_plate
                    ]
                        .filter(
                            value =>
                                value !== null &&
                                value !== undefined
                        )
                        .join(" ")
                        .toLowerCase();


                return searchable.includes(
                    keyword
                );
            });


        renderVehicles(filtered);
    }


    /* ========================================================
       STATUS
       ======================================================== */

    function normalizeStatus(status) {

        if (!status) {
            return "UNKNOWN";
        }


        return String(status)
            .trim()
            .toUpperCase();
    }


    function createStatusBadge(status) {

        let cssClass =
            "status-unknown";

        let label =
            status;


        switch (status) {

            case "IN_TRIP":

                cssClass =
                    "status-trip";

                label =
                    "Đang trong chuyến";

                break;


            case "ACTIVE":

                cssClass =
                    "status-active";

                label =
                    "Hoạt động";

                break;


            case "AVAILABLE":

                cssClass =
                    "status-active";

                label =
                    "Sẵn sàng";

                break;


            case "INACTIVE":

                cssClass =
                    "status-inactive";

                label =
                    "Không hoạt động";

                break;


            case "OFFLINE":

                cssClass =
                    "status-inactive";

                label =
                    "Offline";

                break;


            default:

                label =
                    "Không xác định";
        }


        return `

            <span class="status-badge ${cssClass}">

                ${escapeHTML(label)}

            </span>

        `;
    }


    /* ========================================================
       TRIP INFORMATION
       ======================================================== */

    function getTripInformation(vehicle) {

        if (
            vehicle.current_trip
        ) {

            if (
                typeof vehicle.current_trip ===
                "string"
            ) {

                return vehicle.current_trip;
            }


            return (
                vehicle.current_trip.trip_code ??
                vehicle.current_trip.code ??
                vehicle.current_trip.id ??
                null
            );
        }


        return (
            vehicle.trip_code ??
            vehicle.current_trip_code ??
            vehicle.trip_id ??
            null
        );
    }


    /* ========================================================
       GPS INFORMATION
       ======================================================== */

    function hasGPS(vehicle) {

        if (
            Number(vehicle.gps_count ?? 0) > 0
        ) {
            return true;
        }

        if (vehicle.last_gps_at) {
            return true;
        }

        if (
            vehicle.gps_log_id ||
            vehicle.latest_gps_id
        ) {
            return true;
        }

        if (
            vehicle.latitude !== null &&
            vehicle.latitude !== undefined &&
            vehicle.longitude !== null &&
            vehicle.longitude !== undefined
        ) {
            return true;
        }

        if (vehicle.latest_location) {
            return true;
        }

        return false;
    }


    /* ========================================================
       VEHICLE LINK
       ======================================================== */

    function createVehicleLink(deviceId) {

        if (
            !deviceId ||
            deviceId === "--"
        ) {

            return "/dashboard";
        }


        return (
            "/dashboard?device_id=" +
            encodeURIComponent(deviceId)
        );
    }


    /* ========================================================
       SERVER STATUS
       ======================================================== */

    function setServerStatus(
        state,
        message
    ) {

        serverStatus.textContent =
            message;


        statusDot.classList.remove(
            "online",
            "offline"
        );


        if (
            state === "online"
        ) {

            statusDot.classList.add(
                "online"
            );

        } else if (
            state === "offline"
        ) {

            statusDot.classList.add(
                "offline"
            );
        }
    }


    /* ========================================================
       LAST UPDATE
       ======================================================== */

    function updateLastUpdate() {

        const now =
            new Date();


        lastUpdate.textContent =
            "Cập nhật: " +
            now.toLocaleTimeString(
                "vi-VN",
                {
                    hour: "2-digit",
                    minute: "2-digit",
                    second: "2-digit"
                }
            );
    }


    /* ========================================================
       LOADING
       ======================================================== */

    function setLoading(isLoading) {

        refreshButton.disabled =
            isLoading;


        refreshButton.textContent =
            isLoading
                ? "Đang tải..."
                : "Cập nhật";
    }


    /* ========================================================
       LOAD ERROR
       ======================================================== */

    function showLoadError() {

        vehicleTableBody.innerHTML = `

            <tr>

                <td colspan="6">

                    <div class="loading">

                        Không thể tải danh sách phương tiện.

                    </div>

                </td>

            </tr>

        `;


        tableDescription.textContent =
            "Không thể tải dữ liệu.";
    }


    /* ========================================================
       HTML ESCAPE
       Prevent HTML injection from API data
       ======================================================== */

    function escapeHTML(value) {

        if (
            value === null ||
            value === undefined
        ) {

            return "";
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


    /* ========================================================
       EVENTS
       ======================================================== */

    refreshButton.addEventListener(
        "click",
        loadVehicles
    );


    searchInput.addEventListener(
        "input",
        filterVehicles
    );


    /* ========================================================
       INITIAL LOAD
       ======================================================== */

    loadVehicles();

});