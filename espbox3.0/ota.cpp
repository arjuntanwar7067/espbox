#include "ota.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// OTA State
bool otaActive = false;
String otaStatus = "Idle";
float otaProgress = 0;
String otaError = "";
unsigned long otaStartTime = 0;
bool otaInitialized = false;
bool otaAPMode = false;

// OTA AP Credentials
const char* OTA_AP_SSID = "OTA_Watch";
const char* OTA_AP_PASSWORD = "12345678";
String otaHostname = "ESP32-OTA";

// WiFi credentials storage
WiFiCreds savedWiFi;

// =================================================
// OTA CALLBACKS
// =================================================

void onOTAStart() {
    otaStatus = "OTA Start";
    otaProgress = 0;
    otaStartTime = millis();
    Serial.println("OTA Update Started!");
    showPopupMessage("📦 OTA Started", "Uploading firmware...", "");
    delay(500);
}

void onOTAProgress(unsigned int progress, unsigned int total) {
    otaProgress = (float)progress / total * 100;
    if ((int)otaProgress % 10 == 0) {
        Serial.printf("OTA Progress: %d%%\n", (int)otaProgress);
        char progStr[20];
        sprintf(progStr, "%d%%", (int)otaProgress);
        showPopupMessage("📦 Updating...", progStr, "");
    }
}

void onOTAEnd() {
    otaStatus = "OTA Complete! Rebooting...";
    otaProgress = 100;
    Serial.println("OTA Update Complete! Rebooting...");
    showPopupMessage("✅ OTA Complete!", "Rebooting...", "");
    delay(1000);
    ESP.restart();
}

void onOTAError(ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    switch(error) {
        case OTA_AUTH_ERROR:
            otaError = "Auth Failed";
            Serial.println("Auth Failed");
            break;
        case OTA_BEGIN_ERROR:
            otaError = "Begin Failed";
            Serial.println("Begin Failed");
            break;
        case OTA_CONNECT_ERROR:
            otaError = "Connect Failed";
            Serial.println("Connect Failed");
            break;
        case OTA_RECEIVE_ERROR:
            otaError = "Receive Failed";
            Serial.println("Receive Failed");
            break;
        case OTA_END_ERROR:
            otaError = "End Failed";
            Serial.println("End Failed");
            break;
        default:
            otaError = "Unknown";
            Serial.println("Unknown Error");
    }
    showPopupMessage("❌ OTA Error!", otaError.c_str(), "");
    delay(1500);
    otaStatus = "Error: " + otaError;
}

// =================================================
// LOAD/SAVE WIFI CREDENTIALS
// =================================================

void loadWiFiCredentials() {
    EEPROM.begin(512);
    EEPROM.get(200, savedWiFi);
    EEPROM.end();
    
    // ✅ Check if credentials are valid (not corrupted)
    bool isValid = savedWiFi.valid;
    if (isValid) {
        // Check if SSID contains printable characters
        bool hasValidChars = false;
        for (int i = 0; i < 32 && savedWiFi.ssid[i] != '\0'; i++) {
            if (savedWiFi.ssid[i] >= 32 && savedWiFi.ssid[i] <= 126) {
                hasValidChars = true;
                break;
            }
        }
        if (!hasValidChars) {
            isValid = false;
            Serial.println("⚠️ Corrupted WiFi credentials detected!");
        }
    }
    
    if (isValid && strlen(savedWiFi.ssid) > 0) {
        Serial.println("📂 Loaded WiFi credentials");
        Serial.printf("  SSID: %s\n", savedWiFi.ssid);
    } else {
        Serial.println("📂 No valid WiFi credentials saved");
        // Clear corrupted data
        savedWiFi.valid = false;
        memset(savedWiFi.ssid, 0, 32);
        memset(savedWiFi.password, 0, 64);
        // Save cleared state
        EEPROM.begin(512);
        EEPROM.put(200, savedWiFi);
        EEPROM.commit();
        EEPROM.end();
    }
}

void saveWiFiCredentials(const char* ssid, const char* password) {
    EEPROM.begin(512);
    memset(savedWiFi.ssid, 0, 32);
    memset(savedWiFi.password, 0, 64);
    strncpy(savedWiFi.ssid, ssid, 31);
    savedWiFi.ssid[31] = '\0';
    strncpy(savedWiFi.password, password, 63);
    savedWiFi.password[63] = '\0';
    savedWiFi.valid = true;
    EEPROM.put(200, savedWiFi);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("💾 WiFi credentials saved");
    Serial.printf("  SSID: %s\n", savedWiFi.ssid);
}

void clearWiFiCredentials() {
    EEPROM.begin(512);
    savedWiFi.valid = false;
    memset(savedWiFi.ssid, 0, 32);
    memset(savedWiFi.password, 0, 64);
    EEPROM.put(200, savedWiFi);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("🗑️ WiFi credentials cleared");
    showPopupMessage("🗑️ WiFi Cleared!", "", "");
    delay(1000);
}

// =================================================
// CONNECT TO WIFI (STA Mode)
// =================================================

bool connectToWiFi() {
    if (!savedWiFi.valid) {
        Serial.println("❌ No WiFi credentials saved");
        showPopupMessage("❌ No WiFi Saved!", "Use Settings to set", "");
        delay(1500);
        return false;
    }
    
    // Check if SSID has valid characters
    bool hasValidChars = false;
    for (int i = 0; i < 32 && savedWiFi.ssid[i] != '\0'; i++) {
        if (savedWiFi.ssid[i] >= 32 && savedWiFi.ssid[i] <= 126) {
            hasValidChars = true;
            break;
        }
    }
    
    if (!hasValidChars || strlen(savedWiFi.ssid) == 0) {
        Serial.println("❌ Invalid WiFi credentials!");
        clearWiFiCredentials();
        showPopupMessage("❌ Invalid Credentials!", "Please re-enter", "");
        delay(1500);
        return false;
    }
    
    Serial.printf("📡 Connecting to WiFi: %s\n", savedWiFi.ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedWiFi.ssid, savedWiFi.password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        attempts++;
        Serial.print(".");
        if (attempts % 5 == 0) {
            char msg[30];
            sprintf(msg, "Connecting... %d/30", attempts);
            showPopupMessage("📡 WiFi", msg, savedWiFi.ssid);
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi Connected!");
        Serial.printf("📡 IP: %s\n", WiFi.localIP().toString().c_str());
        showPopupMessage("✅ WiFi Connected!", WiFi.localIP().toString().c_str(), "OTA Ready");
        delay(1000);
        otaAPMode = false;
        return true;
    } else {
        Serial.println("\n❌ WiFi Connection Failed!");
        showPopupMessage("❌ WiFi Failed!", "Check credentials", "");
        delay(1000);
        return false;
    }
}

// =================================================
// START OTA IN AP MODE
// =================================================

void startOTAAPMode() {
    if (otaActive) return;
    
    Serial.println("📦 Starting OTA in AP Mode...");
    
    // Stop any existing WiFi
    WiFi.disconnect(true);
    delay(100);
    
    // Start AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(OTA_AP_SSID, OTA_AP_PASSWORD);
    delay(100);
    
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("📡 AP Started: %s\n", OTA_AP_SSID);
    Serial.printf("🔑 Password: %s\n", OTA_AP_PASSWORD);
    Serial.printf("🌐 IP: %s\n", ip.toString().c_str());
    
    otaAPMode = true;
    
    // Set hostname for MDNS
    otaHostname = "ESP32-OTA";
    ArduinoOTA.setHostname(otaHostname.c_str());
    
    // Configure OTA callbacks
    ArduinoOTA.onStart(onOTAStart);
    ArduinoOTA.onProgress(onOTAProgress);
    ArduinoOTA.onEnd(onOTAEnd);
    ArduinoOTA.onError(onOTAError);
    
    // Start OTA
    ArduinoOTA.begin();
    otaActive = true;
    otaInitialized = true;
    otaStatus = "OTA Running (AP Mode)";
    
    Serial.println("========================================");
    Serial.println("📦 OTA READY IN AP MODE!");
    Serial.printf("📡 SSID: %s\n", OTA_AP_SSID);
    Serial.printf("🔑 Password: %s\n", OTA_AP_PASSWORD);
    Serial.printf("📡 Hostname: %s.local\n", otaHostname.c_str());
    Serial.printf("🌐 IP: %s\n", ip.toString().c_str());
    Serial.println("📤 In Arduino IDE: Tools → Port → Network ports");
    Serial.printf("📤 Select: %s at %s\n", otaHostname.c_str(), ip.toString().c_str());
    Serial.println("========================================");
    
    String msg1 = "SSID: " + String(OTA_AP_SSID);
    String msg2 = otaHostname + ".local";
    showPopupMessage("📦 OTA Ready!", msg1.c_str(), msg2.c_str());
    delay(2000);
}

// =================================================
// START OTA IN STA MODE
// =================================================

void startOTASTAMode() {
    if (otaActive) return;
    
    if (WiFi.status() != WL_CONNECTED) {
        showPopupMessage("❌ WiFi Not Connected!", "Connect to WiFi first", "");
        delay(1500);
        return;
    }
    
    Serial.println("📦 Starting OTA in STA Mode...");
    
    // Set hostname
    otaHostname = "ESP32-OTA";
    ArduinoOTA.setHostname(otaHostname.c_str());
    
    // Configure OTA callbacks
    ArduinoOTA.onStart(onOTAStart);
    ArduinoOTA.onProgress(onOTAProgress);
    ArduinoOTA.onEnd(onOTAEnd);
    ArduinoOTA.onError(onOTAError);
    
    // Start OTA
    ArduinoOTA.begin();
    otaActive = true;
    otaInitialized = true;
    otaStatus = "OTA Running (STA Mode)";
    otaAPMode = false;
    
    String ipStr = WiFi.localIP().toString();
    Serial.println("========================================");
    Serial.println("📦 OTA READY IN STA MODE!");
    Serial.printf("📡 Hostname: %s.local\n", otaHostname.c_str());
    Serial.printf("🌐 IP: %s\n", ipStr.c_str());
    Serial.println("📤 In Arduino IDE: Tools → Port → Network ports");
    Serial.printf("📤 Select: %s at %s\n", otaHostname.c_str(), ipStr.c_str());
    Serial.println("========================================");
    
    String msg1 = otaHostname + ".local";
    showPopupMessage("📦 OTA Ready!", msg1.c_str(), ipStr.c_str());
    delay(2000);
}

void stopOTA() {
    if (otaActive) {
        ArduinoOTA.end();
        otaActive = false;
        otaStatus = "OTA Stopped";
        Serial.println("⏹️ OTA stopped");
        
        if (otaAPMode) {
            WiFi.softAPdisconnect(true);
            otaAPMode = false;
            WiFi.mode(WIFI_OFF);
            Serial.println("  ✅ AP disconnected");
        }
        showPopupMessage("⏹️ OTA Stopped", "", "");
        delay(500);
    }
}

// =================================================
// DRAW OTA MENU
// =================================================

void drawOTAMenu() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_ota);
    
    CenteredStr(12, "📦 OTA Update");
    u8g2.drawLine(0, 14, 128, 14);
    
    // Mode status
    if (otaAPMode) {
        u8g2.setCursor(2, 24);
        u8g2.print("📡 AP Mode: ");
        u8g2.print(OTA_AP_SSID);
        u8g2.setCursor(2, 34);
        u8g2.print("🔑 Pass: ");
        u8g2.print(OTA_AP_PASSWORD);
        u8g2.setCursor(2, 44);
        u8g2.print("🌐 IP: ");
        u8g2.print(WiFi.softAPIP().toString().c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        u8g2.setCursor(2, 24);
        u8g2.print("📡 WiFi: Connected");
        u8g2.setCursor(2, 34);
        u8g2.print("IP: ");
        u8g2.print(WiFi.localIP().toString().c_str());
    } else {
        u8g2.setCursor(2, 24);
        u8g2.print("📡 WiFi: Disconnected");
    }
    
    // OTA Status
    u8g2.setCursor(2, 54);
    u8g2.print("Status: ");
    u8g2.print(otaStatus.c_str());
    
    // Progress bar
    if (otaProgress > 0 && otaProgress < 100) {
        u8g2.drawFrame(2, 50, 124, 8);
        u8g2.drawBox(4, 52, (int)(otaProgress * 1.2), 4);
        char progStr[10];
        sprintf(progStr, "%d%%", (int)otaProgress);
        u8g2.setCursor(55, 60);
        u8g2.print(progStr);
    } else if (otaProgress == 100) {
        u8g2.drawFrame(2, 50, 124, 8);
        u8g2.drawBox(4, 52, 120, 4);
        u8g2.setCursor(50, 60);
        u8g2.print("✅ Complete!");
    }
    
    // Bottom hints
    u8g2.setCursor(0, 63);
    if (otaActive) {
        u8g2.print("🔴 OTA Active  BACK:Exit");
    } else {
        u8g2.print("SEL:Start AP  SEL+DN:STA  BACK:Exit");
    }
    
    u8g2.sendBuffer();
}

// =================================================
// HANDLE SERIAL COMMANDS
// =================================================

void handleSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.startsWith("WIFI:")) {
            String creds = cmd.substring(5);
            int comma = creds.indexOf(',');
            if (comma > 0) {
                String ssid = creds.substring(0, comma);
                String password = creds.substring(comma + 1);
                ssid.trim();
                password.trim();
                if (ssid.length() > 0 && password.length() > 0) {
                    saveWiFiCredentials(ssid.c_str(), password.c_str());
                    Serial.println("✅ WiFi credentials saved!");
                    Serial.printf("  SSID: %s\n", ssid.c_str());
                    showPopupMessage("✅ WiFi Saved!", ssid.c_str(), "");
                    delay(1000);
                    connectToWiFi();
                }
            } else {
                Serial.println("❌ Invalid format! Use: WIFI:SSID,Password");
                Serial.println("Example: WIFI:MyHomeWiFi,MyPassword123");
            }
        } else if (cmd == "WIFI:CLEAR") {
            clearWiFiCredentials();
            showPopupMessage("🗑️ WiFi Cleared!", "", "");
            delay(1000);
        } else if (cmd == "WIFI:STATUS") {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("✅ WiFi Connected");
                Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
                Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
            } else {
                Serial.println("❌ WiFi Disconnected");
                if (savedWiFi.valid) {
                    Serial.printf("  Saved SSID: %s\n", savedWiFi.ssid);
                } else {
                    Serial.println("  No credentials saved");
                }
            }
        } else if (cmd == "OTA:STARTAP") {
            startOTAAPMode();
        } else if (cmd == "OTA:STARTSTA") {
            startOTASTAMode();
        } else if (cmd == "OTA:STOP") {
            stopOTA();
        } else if (cmd == "HELP") {
            Serial.println("\n=== COMMANDS ===");
            Serial.println("WIFI:SSID,Password  - Save WiFi credentials");
            Serial.println("WIFI:CLEAR           - Clear WiFi credentials");
            Serial.println("WIFI:STATUS          - Show WiFi status");
            Serial.println("OTA:STARTAP          - Start OTA in AP mode");
            Serial.println("OTA:STARTSTA         - Start OTA in STA mode");
            Serial.println("OTA:STOP             - Stop OTA server");
            Serial.println("HELP                 - Show this help");
            Serial.println("==================\n");
        }
    }
}

// =================================================
// OTA SETUP
// =================================================

void otaSetup() {
    otaActive = false;
    otaInitialized = false;
    otaStatus = "Idle";
    otaProgress = 0;
    otaError = "";
    otaAPMode = false;
    clearPopup();
    
    // ✅ Clear any corrupted credentials first
    loadWiFiCredentials();
    
    // Print help
    Serial.println("\n========================================");
    Serial.println("📦 OTA UPDATE MODULE");
    Serial.println("========================================");
    Serial.println("To set WiFi credentials, type in Serial Monitor:");
    Serial.println("  WIFI:YourSSID,YourPassword");
    Serial.println("  Example: WIFI:MyHomeWiFi,MyPassword123");
    Serial.println("");
    Serial.println("Other commands:");
    Serial.println("  WIFI:CLEAR    - Clear saved credentials");
    Serial.println("  WIFI:STATUS   - Show WiFi status");
    Serial.println("  OTA:STARTAP   - Start OTA in AP mode");
    Serial.println("  OTA:STARTSTA  - Start OTA in STA mode");
    Serial.println("  OTA:STOP      - Stop OTA server");
    Serial.println("  HELP          - Show this help");
    Serial.println("========================================\n");
    
    // ✅ If no valid credentials, start AP mode automatically
    if (!savedWiFi.valid) {
        Serial.println("📡 No valid WiFi credentials - Starting AP mode...");
        startOTAAPMode();
    } else {
        // Try to connect to saved WiFi
        connectToWiFi();
        // If connection fails, start AP mode
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("📡 WiFi connection failed - Starting AP mode...");
            startOTAAPMode();
        }
    }
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_ota);
    CenteredStr(20, "📦 OTA Update");
    
    if (otaAPMode) {
        CenteredStr(35, "📡 AP Mode: OTA_Watch");
        CenteredStr(48, "🔑 Pass: 12345678");
        CenteredStr(58, "Connect & Upload via IDE");
    } else if (WiFi.status() == WL_CONNECTED) {
        CenteredStr(35, "✅ WiFi Connected");
        CenteredStr(48, WiFi.localIP().toString().c_str());
        CenteredStr(58, "SEL:Start AP  SEL+DN:STA");
    } else {
        CenteredStr(35, "📡 AP Mode Available");
        CenteredStr(48, "SELECT: Start AP Mode");
        CenteredStr(58, "SSID: OTA_Watch");
    }
    CenteredStr(63, "BACK: Exit");
    u8g2.sendBuffer();
}

// =================================================
// OTA LOOP
// =================================================

void otaLoop() {
    if (drawPopupIfActive()) {
        delay(50);
        return;
    }
    
    handleSerialCommands();
    
    if (otaActive && otaInitialized) {
        ArduinoOTA.handle();
    }
    
    drawOTAMenu();
    
    if (isButtonPressed(BTN_BACK)) {
        if (otaActive) {
            stopOTA();
        }
        exitModule();
        return;
    }
    
    // SELECT: Start AP Mode
    if (isButtonPressed(BTN_SELECT) && !isButtonPressed(BTN_DOWN)) {
        if (!otaActive) {
            startOTAAPMode();
            drawOTAMenu();
        } else {
            showPopupMessage("📦 OTA Active", "SSID: OTA_Watch", "Connect & Upload");
            delay(1500);
        }
    }
    
    // SELECT + DOWN: Start STA Mode (if WiFi connected)
    if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_DOWN)) {
        if (!otaActive) {
            if (WiFi.status() == WL_CONNECTED) {
                startOTASTAMode();
                drawOTAMenu();
            } else {
                showPopupMessage("❌ WiFi Not Connected!", "Connect to WiFi first", "");
                delay(1500);
            }
        } else {
            showPopupMessage("📦 OTA Active", "", "");
            delay(1000);
        }
    }
    
    delay(20);
}

// =================================================
// OTA CLEANUP
// =================================================

void otaCleanup() {
    Serial.println("🧹 OTA cleanup...");
    unsigned long startTime = millis();
    
    if (otaActive) {
        stopOTA();
    }
    
    if (otaAPMode) {
        WiFi.softAPdisconnect(true);
        otaAPMode = false;
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    otaActive = false;
    otaInitialized = false;
    otaStatus = "Idle";
    otaProgress = 0;
    otaError = "";
    clearPopup();
    
    Serial.printf("✅ OTA cleanup complete (%lu ms)\n", millis() - startTime);
}