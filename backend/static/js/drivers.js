/* ============================================================
   SMARTFLEET - DRIVERS PAGE
   ============================================================ */

document.addEventListener("DOMContentLoaded", () => {

    const DRIVERS_API = "/api/drivers";


    /* ========================================================
       ELEMENTS
       ======================================================== */

    const driverTableBody =
        document.getElementById("driverTableBody");

    const searchInput =
        document.getElementById("searchInput");

    const refreshButton =
        document.getElementById("refreshButton");

    const totalDrivers =
        document.getElementById("totalDrivers");

    const activeDrivers =
        document.getElementById("activeDrivers");

    const drivingDrivers =
        document.getElementById("drivingDrivers");

    const inactiveDrivers =
        document.getElementById("inactiveDrivers");

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

    let drivers = [];


    /* ========================================================
       LOAD DRIVERS
       ======================================================== */

    async function loadDrivers() {

        setLoading(true);

        setServerStatus(
            "loading",
            "Đang kết nối SmartFleet Server..."
        );


        try {

            const response =
                await fetch(
                    DRIVERS_API,
                    {
                        method: "GET",

                        headers: {
                            "Accept": "application/json"
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
             * Hỗ trợ:
             *
             * { "drivers": [...] }
             * { "data": [...] }
             * [...]
             */

            if (Array.isArray(data)) {

                drivers = data;

            } else if (
                Array.isArray(data.drivers)
            ) {

                drivers = data.drivers;

            } else if (
                Array.isArray(data.data)
            ) {

                drivers = data.data;

            } else {

                drivers = [];
            }


            updateSummary(drivers);

            renderDrivers(drivers);


            setServerStatus(
                "online",
                "SmartFleet Server đã kết nối"
            );


            updateLastUpdate();


        } catch (error) {

            console.error(
                "Load drivers error:",
                error
            );


            drivers = [];


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
       RENDER
       ======================================================== */

    function renderDrivers(driverList) {

        driverTableBody.innerHTML = "";


        if (
            !driverList ||
            driverList.length === 0
        ) {

            emptyState.classList.add("show");

            tableDescription.textContent =
                "Không có tài xế để hiển thị.";

            return;
        }


        emptyState.classList.remove("show");


        tableDescription.textContent =
            `${driverList.length} tài xế`;


        driverList.forEach((driver) => {

            const row =
                document.createElement("tr");


            const id =
                driver.id ?? "--";


            const driverCode =
                driver.driver_code ??
                driver.code ??
                `Driver ${id}`;


            const fullName =
                driver.full_name ??
                driver.name ??
                driver.user_name ??
                "--";


            const rfid =
                getRFID(driver);


            const status =
                normalizeStatus(
                    driver.status
                );


            const trip =
                getTrip(driver);


            row.innerHTML = `

                <td>

                    <div class="driver-info">

                        <strong>
                            ${escapeHTML(driverCode)}
                        </strong>

                        <span>
                            ID: ${escapeHTML(id)}
                        </span>

                    </div>

                </td>


                <td>
                    ${escapeHTML(fullName)}
                </td>


                <td>

                    ${
                        rfid
                            ? `
                                <span class="rfid-code">
                                    ${escapeHTML(rfid)}
                                </span>
                              `
                            : "--"
                    }

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

                    <a
                        class="view-button"
                        href="${createDriverLink(id)}"
                    >
                        Chi tiết
                    </a>

                </td>
            `;


            driverTableBody.appendChild(row);
        });
    }


    /* ========================================================
       SUMMARY
       ======================================================== */

    function updateSummary(driverList) {

        const total =
            driverList.length;


        let active = 0;
        let driving = 0;
        let inactive = 0;


        driverList.forEach((driver) => {

            const status =
                normalizeStatus(
                    driver.status
                );


            if (
                status === "IN_TRIP" ||
                status === "DRIVING"
            ) {

                driving++;

            } else if (
                status === "ACTIVE" ||
                status === "AVAILABLE"
            ) {

                active++;

            } else {

                inactive++;
            }
        });


        totalDrivers.textContent =
            total;

        activeDrivers.textContent =
            active;

        drivingDrivers.textContent =
            driving;

        inactiveDrivers.textContent =
            inactive;
    }


    /* ========================================================
       SEARCH
       ======================================================== */

    function filterDrivers() {

        const keyword =
            searchInput.value
                .trim()
                .toLowerCase();


        if (!keyword) {

            renderDrivers(drivers);

            return;
        }


        const filtered =
            drivers.filter((driver) => {

                const searchable =
                    [
                        driver.id,
                        driver.driver_code,
                        driver.code,
                        driver.full_name,
                        driver.name,
                        driver.status,
                        driver.rfid_uid,
                        driver.rfid,
                        driver.card_uid
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


        renderDrivers(filtered);
    }


    /* ========================================================
       RFID
       ======================================================== */

    function getRFID(driver) {

        if (driver.rfid_uid) {
            return driver.rfid_uid;
        }


        if (driver.rfid) {

            if (
                typeof driver.rfid === "string"
            ) {

                return driver.rfid;
            }


            return (
                driver.rfid.uid ??
                driver.rfid.card_uid ??
                null
            );
        }


        if (driver.card_uid) {
            return driver.card_uid;
        }


        return null;
    }


    /* ========================================================
       TRIP
       ======================================================== */

    function getTrip(driver) {

        if (driver.current_trip) {

            if (
                typeof driver.current_trip ===
                "string"
            ) {

                return driver.current_trip;
            }


            return (
                driver.current_trip.trip_code ??
                driver.current_trip.code ??
                driver.current_trip.id ??
                null
            );
        }


        return (
            driver.trip_code ??
            driver.current_trip_code ??
            driver.trip_id ??
            null
        );
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
            "Không xác định";


        switch (status) {

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


            case "IN_TRIP":

            case "DRIVING":

                cssClass =
                    "status-driving";

                label =
                    "Đang trong chuyến";

                break;


            case "INACTIVE":

            case "OFFLINE":

                cssClass =
                    "status-inactive";

                label =
                    "Không hoạt động";

                break;
        }


        return `

            <span class="status-badge ${cssClass}">

                ${escapeHTML(label)}

            </span>

        `;
    }


    /* ========================================================
       DRIVER LINK
       ======================================================== */

    function createDriverLink(driverId) {

        if (
            !driverId ||
            driverId === "--"
        ) {

            return "/drivers";
        }


        return (
            "/drivers/" +
            encodeURIComponent(driverId)
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
       ERROR
       ======================================================== */

    function showLoadError() {

        driverTableBody.innerHTML = `

            <tr>

                <td colspan="6">

                    <div class="loading">

                        Không thể tải danh sách tài xế.

                    </div>

                </td>

            </tr>

        `;


        tableDescription.textContent =
            "Không thể tải dữ liệu.";
    }


    /* ========================================================
       HTML ESCAPE
       ======================================================== */

    function escapeHTML(value) {

        if (
            value === null ||
            value === undefined
        ) {

            return "";
        }


        return String(value)
            .replaceAll("&", "&amp;")
            .replaceAll("<", "&lt;")
            .replaceAll(">", "&gt;")
            .replaceAll('"', "&quot;")
            .replaceAll("'", "&#039;");
    }


    /* ========================================================
       EVENTS
       ======================================================== */

    refreshButton.addEventListener(
        "click",
        loadDrivers
    );


    searchInput.addEventListener(
        "input",
        filterDrivers
    );


    /* ========================================================
       START
       ======================================================== */

    loadDrivers();

});