const loginForm = document.getElementById("loginForm");
const loginButton = document.getElementById("loginButton");
const loginError = document.getElementById("loginError");

loginForm.addEventListener("submit", async (event) => {

    event.preventDefault();

    loginError.textContent = "";
    loginButton.disabled = true;
    loginButton.textContent = "Đang đăng nhập...";

    const email =
        document.getElementById("email").value.trim();

    const password =
        document.getElementById("password").value;

    try {

        const response = await fetch(
            "/api/auth/login",
            {
                method: "POST",

                headers: {
                    "Content-Type": "application/json"
                },

                credentials: "same-origin",

                body: JSON.stringify({
                    email: email,
                    password: password
                })
            }
        );

        const data = await response.json();

        if (response.ok && data.authenticated === true) {

            window.location.href = "/dashboard";
            return;
        }

        if (data.reason === "INVALID_CREDENTIALS") {

            loginError.textContent =
                "Email hoặc mật khẩu không chính xác.";

        } else if (data.reason === "ACCOUNT_NOT_ACTIVE") {

            loginError.textContent =
                "Tài khoản hiện không hoạt động.";

        } else if (data.reason === "ADMIN_REQUIRED") {

            loginError.textContent =
                "Tài khoản không có quyền quản trị.";

        } else {

            loginError.textContent =
                "Không thể đăng nhập. Vui lòng thử lại.";
        }

    } catch (error) {

        console.error(error);

        loginError.textContent =
            "Không thể kết nối tới máy chủ.";

    } finally {

        loginButton.disabled = false;
        loginButton.textContent = "Đăng nhập";
    }
});