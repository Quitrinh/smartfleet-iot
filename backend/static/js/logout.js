/* ============================================================
   SMARTFLEET
   Admin Logout
   ============================================================ */

"use strict";


/* ============================================================
   LOGOUT ADMIN
   ============================================================ */

async function logoutAdmin() {

    const logoutButton =
        document.getElementById("logoutButton");

    if (logoutButton) {
        logoutButton.disabled = true;

        logoutButton.innerHTML = `
            <i class="fa-solid fa-spinner fa-spin"></i>
            <span>Đang đăng xuất...</span>
        `;
    }

    try {

        const response = await fetch("/logout", {
            method: "POST",
            credentials: "same-origin"
        });

        const data = await response.json();

        if (
            response.ok &&
            data.status === "ok" &&
            data.logged_out === true
        ) {
            window.location.replace("/login");
            return;
        }

        console.error("Logout failed:", data);

        alert(
            "Không thể đăng xuất. Vui lòng thử lại."
        );

    } catch (error) {

        console.error(
            "Logout request error:",
            error
        );

        alert(
            "Không thể kết nối tới máy chủ."
        );

    } finally {

        if (logoutButton) {

            logoutButton.disabled = false;

            logoutButton.innerHTML = `
                <i class="fa-solid fa-right-from-bracket"></i>
                <span>Đăng xuất</span>
            `;
        }
    }
}


/* ============================================================
   BUTTON EVENT
   ============================================================ */

document.addEventListener(
    "DOMContentLoaded",
    () => {

        const logoutButton =
            document.getElementById(
                "logoutButton"
            );

        if (logoutButton) {

            logoutButton.addEventListener(
                "click",
                logoutAdmin
            );
        }
    }
);