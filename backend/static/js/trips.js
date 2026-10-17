/* ============================================================
   SMARTFLEET - TRIPS PAGE
   File: backend/static/js/trips.js
   ============================================================ */

document.addEventListener("DOMContentLoaded", () => {

    /* ========================================================
       CONFIG
       ======================================================== */

    const TRIPS_API = "/api/trips";


    /* ========================================================
       DOM ELEMENTS
       ======================================================== */

    const tableBody =
        document.getElementById("tripTableBody");

    const searchInput =
        document.getElementById("searchInput");

    const refreshButton =
        document.getElementById("refreshButton");

    const totalTrips =
        document.getElementById("totalTrips");

    const activeTrips =
        document.getElementById("activeTrips");

    const completedTrips =
        document.getElementById("completedTrips");

    const cancelledTrips =
        document.getElementById("cancelledTrips");

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

    let trips = [];


    /* ========================================================
       LOAD TRIPS
       ======================================================== */

    async function loadTrips() {

        setLoading(true);

        hideEmptyState();

        setSummaryLoading();

        setServerStatus(
            "loading",
            "Đang kết nối SmartFleet Server..."
        );


        try {

            const response = await fetch(
                TRIPS_API,
                {
                    method: "GET",

                    headers: {
                        "Accept": "application/json"
                    },

                    cache: "no-store"
                }
            );


            console.log(
                "[TRIPS] HTTP STATUS:",
                response.status
            );


            if (!response.ok) {

                throw new Error(
                    `HTTP ${response.status}`
                );
            }


            const data =
                await response.json();


            console.log(
                "[TRIPS] API RESPONSE:",
                data
            );


            /* =================================================
               API RESPONSE FORMAT

               Có thể là:

               [
                   {...}
               ]

               hoặc:

               {
                   "trips": [...]
               }

               hoặc:

               {
                   "data": [...]
               }
               ================================================= */

            if (Array.isArray(data)) {

                trips = data;

            } else if (
                data &&
                Array.isArray(data.trips)
            ) {

                trips = data.trips;

            } else if (
                data &&
                Array.isArray(data.data)
            ) {

                trips = data.data;

            } else {

                trips = [];
            }


            /* =================================================
               API ĐÃ KẾT NỐI THÀNH CÔNG
               ================================================= */

            updateSummary(trips);

            renderTrips(trips);

            setServerStatus(
                "online",
                "SmartFleet Server đã kết nối"
            );

            updateTime();


        } catch (error) {

            /* =================================================
               API / DATABASE ERROR
               ================================================= */

            console.error(
                "[TRIPS ERROR]",
                error
            );


            trips = [];


            /*
             * QUAN TRỌNG:
             *
             * Không gọi updateSummary([])
             *
             * Vì API lỗi KHÔNG đồng nghĩa
             * database có 0 chuyến.
             */

            setSummaryError();

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
       RENDER TRIPS
       ======================================================== */

    function renderTrips(list) {

        tableBody.innerHTML = "";


        /* =====================================================
           API OK NHƯNG KHÔNG CÓ DATA
           ===================================================== */

        if (
            !Array.isArray(list) ||
            list.length === 0
        ) {

            tableDescription.textContent =
                "Chưa có chuyến đi.";

            showEmptyState();

            return;
        }


        /* =====================================================
           API OK + CÓ DATA
           ===================================================== */

        hideEmptyState();


        tableDescription.textContent =
            `${list.length} chuyến đi`;


        list.forEach((trip) => {

            const row =
                document.createElement("tr");


            /* =================================================
               TRIP ID
               ================================================= */

            const id =
                trip.id ??
                trip.trip_id ??
                "--";


            /* =================================================
               TRIP CODE
               ================================================= */

            const tripCode =
                trip.trip_code ??
                trip.code ??
                (
                    id !== "--"
                        ? `TRIP_${id}`
                        : "--"
                );


            /* =================================================
               VEHICLE
               ================================================= */

            const vehicle =
                getVehicle(trip);


            /* =================================================
               DRIVER
               ================================================= */

            const driver =
                getDriver(trip);


            /* =================================================
               STATUS
               ================================================= */

            const status =
                normalizeStatus(
                    trip.status
                );


            /* =================================================
               START TIME
               ================================================= */

            const startTime =
                formatDate(
                    trip.started_at ??
                    trip.start_time ??
                    trip.created_at
                );


            /* =================================================
               CREATE ROW
               ================================================= */

            row.innerHTML = `

                <td>

                    <span class="trip-code">
                        ${escapeHTML(tripCode)}
                    </span>

                </td>


                <td>
                    ${escapeHTML(vehicle)}
                </td>


                <td>
                    ${escapeHTML(driver)}
                </td>


                <td>
                    ${createStatus(status)}
                </td>


                <td>

                    <span class="trip-time">
                        ${escapeHTML(startTime)}
                    </span>

                </td>


                <td>

                    ${
                        id !== "--"
                            ? `
                                <a
                                    class="view-button"
                                    href="/dashboard?trip_id=${encodeURIComponent(id)}"
                                >
                                    Xem GPS
                                </a>
                              `
                            : "--"
                    }

                </td>


                <td>

                    ${
                        id !== "--"
                            ? `
                                <a
                                    class="view-button"
                                    href="/trips/${encodeURIComponent(id)}"
                                >
                                    Chi tiết
                                </a>
                              `
                            : "--"
                    }

                </td>
            `;


            tableBody.appendChild(
                row
            );
        });
    }


    /* ========================================================
       GET VEHICLE
       ======================================================== */

    function getVehicle(trip) {

        if (trip.vehicle_code) {
            return trip.vehicle_code;
        }


        if (trip.device_id) {
            return trip.device_id;
        }


        if (trip.vehicle_name) {
            return trip.vehicle_name;
        }


        if (
            trip.vehicle &&
            typeof trip.vehicle === "object"
        ) {

            return (
                trip.vehicle.vehicle_code ??
                trip.vehicle.code ??
                trip.vehicle.device_id ??
                trip.vehicle.name ??
                (
                    trip.vehicle.id
                        ? `Vehicle ${trip.vehicle.id}`
                        : "--"
                )
            );
        }


        if (trip.vehicle_id) {

            return `Vehicle ${trip.vehicle_id}`;
        }


        return "--";
    }


    /* ========================================================
       GET DRIVER
       ======================================================== */

    function getDriver(trip) {

        if (trip.driver_name) {
            return trip.driver_name;
        }


        if (trip.driver_code) {
            return trip.driver_code;
        }


        if (trip.full_name) {
            return trip.full_name;
        }


        if (
            trip.driver &&
            typeof trip.driver === "object"
        ) {

            return (
                trip.driver.full_name ??
                trip.driver.name ??
                trip.driver.driver_code ??
                trip.driver.code ??
                (
                    trip.driver.id
                        ? `Driver ${trip.driver.id}`
                        : "--"
                )
            );
        }


        if (trip.driver_id) {

            return `Driver ${trip.driver_id}`;
        }


        return "--";
    }


    /* ========================================================
       SUMMARY - API OK
       ======================================================== */

    function updateSummary(list) {

        let active = 0;

        let completed = 0;

        let cancelled = 0;


        list.forEach((trip) => {

            const status =
                normalizeStatus(
                    trip.status
                );


            /* -------------------------------------------------
               IN PROGRESS
               ------------------------------------------------- */

            if (
                status === "IN_PROGRESS" ||
                status === "ACTIVE" ||
                status === "RUNNING" ||
                status === "STARTED"
            ) {

                active++;

                return;
            }


            /* -------------------------------------------------
               COMPLETED
               ------------------------------------------------- */

            if (
                status === "COMPLETED" ||
                status === "FINISHED" ||
                status === "DONE"
            ) {

                completed++;

                return;
            }


            /* -------------------------------------------------
               CANCELLED
               ------------------------------------------------- */

            if (
                status === "CANCELLED" ||
                status === "CANCELED"
            ) {

                cancelled++;
            }
        });


        totalTrips.textContent =
            list.length;

        activeTrips.textContent =
            active;

        completedTrips.textContent =
            completed;

        cancelledTrips.textContent =
            cancelled;
    }


    /* ========================================================
       SUMMARY - LOADING
       ======================================================== */

    function setSummaryLoading() {

        totalTrips.textContent = "--";

        activeTrips.textContent = "--";

        completedTrips.textContent = "--";

        cancelledTrips.textContent = "--";
    }


    /* ========================================================
       SUMMARY - ERROR
       ======================================================== */

    function setSummaryError() {

        /*
         * API lỗi → không được hiển thị 0.
         *
         * Vì chưa biết database thật có bao nhiêu dữ liệu.
         */

        totalTrips.textContent = "--";

        activeTrips.textContent = "--";

        completedTrips.textContent = "--";

        cancelledTrips.textContent = "--";
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


    function createStatus(status) {

        let cssClass =
            "status-unknown";

        let label =
            "Không xác định";


        switch (status) {

            /* -------------------------------------------------
               ACTIVE
               ------------------------------------------------- */

            case "IN_PROGRESS":

            case "ACTIVE":

            case "RUNNING":

            case "STARTED":

                cssClass =
                    "status-progress";

                label =
                    "Đang thực hiện";

                break;


            /* -------------------------------------------------
               COMPLETED
               ------------------------------------------------- */

            case "COMPLETED":

            case "FINISHED":

            case "DONE":

                cssClass =
                    "status-completed";

                label =
                    "Hoàn thành";

                break;


            /* -------------------------------------------------
               CANCELLED
               ------------------------------------------------- */

            case "CANCELLED":

            case "CANCELED":

                cssClass =
                    "status-cancelled";

                label =
                    "Đã hủy";

                break;


            /* -------------------------------------------------
               UNKNOWN
               ------------------------------------------------- */

            default:

                cssClass =
                    "status-unknown";

                label =
                    status === "UNKNOWN"
                        ? "Không xác định"
                        : status;

                break;
        }


        return `

            <span class="status-badge ${cssClass}">
                ${escapeHTML(label)}
            </span>

        `;
    }


    /* ========================================================
       DATE FORMAT
       ======================================================== */

    function formatDate(value) {

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

            return String(value);
        }


        return date.toLocaleString(
            "vi-VN",
            {
                day: "2-digit",
                month: "2-digit",
                year: "numeric",

                hour: "2-digit",
                minute: "2-digit"
            }
        );
    }


    /* ========================================================
       SEARCH
       ======================================================== */

    function searchTrips() {

        const keyword =
            searchInput.value
                .trim()
                .toLowerCase();


        if (!keyword) {

            renderTrips(trips);

            return;
        }


        const filtered =
            trips.filter((trip) => {

                const text = [

                    trip.id,

                    trip.trip_id,

                    trip.trip_code,

                    trip.code,

                    trip.status,

                    trip.vehicle_id,

                    trip.vehicle_code,

                    trip.device_id,

                    trip.vehicle_name,

                    trip.driver_id,

                    trip.driver_name,

                    trip.driver_code,

                    getVehicle(trip),

                    getDriver(trip)

                ]
                    .filter(
                        value =>
                            value !== null &&
                            value !== undefined
                    )
                    .join(" ")
                    .toLowerCase();


                return text.includes(
                    keyword
                );
            });


        renderTrips(filtered);
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


        if (state === "online") {

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

    function updateTime() {

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
       BUTTON LOADING
       ======================================================== */

    function setLoading(value) {

        refreshButton.disabled =
            value;


        refreshButton.textContent =
            value
                ? "Đang tải..."
                : "Cập nhật";
    }


    /* ========================================================
       EMPTY STATE
       ======================================================== */

    function showEmptyState() {

        emptyState.classList.add(
            "show"
        );
    }


    function hideEmptyState() {

        emptyState.classList.remove(
            "show"
        );
    }


    /* ========================================================
       API ERROR
       ======================================================== */

    function showLoadError() {

        /*
         * API lỗi khác hoàn toàn với:
         *
         * API OK + database không có dữ liệu.
         *
         * Vì vậy KHÔNG hiển thị emptyState.
         */

        hideEmptyState();


        tableBody.innerHTML = `

            <tr>

                <td colspan="7">

                    <div class="loading">

                        Không thể tải dữ liệu từ SmartFleet Server.

                    </div>

                </td>

            </tr>

        `;


        tableDescription.textContent =
            "Không thể tải dữ liệu.";


        lastUpdate.textContent =
            "--";
    }


    /* ========================================================
       ESCAPE HTML
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
        loadTrips
    );


    searchInput.addEventListener(
        "input",
        searchTrips
    );


    /* ========================================================
       START
       ======================================================== */

    loadTrips();

});