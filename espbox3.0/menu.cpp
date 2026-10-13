#include "menu.h"
#include "icons.h"
#include "icons_bruce.h"
#include "utils.h"
#include "config.h"
#include "pins.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// Include all module headers
#include "chat.h"
#include "rogue_ap.h"
#include "wifi_scanner.h"
#include "credentials.h"
#include "settings.h"
#include "about.h"
#include "deauther.h"
#include "evil_portal.h"
#include "marauder_ble.h"
#include "wifi_brute.h"
#include "wifi_beacon_flood.h"
#include "file_manager.h"
#include "packet_monitor.h"
#include "network_mapper.h"
#include "handshake_capture.h"
#include "ir_tools.h"
#include "ota.h"
#include "ble_remote.h"
#include "ereader.h"  // ✅ ADD E-READER HEADER

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern int batteryLevel;
extern String currentTimeStr;
extern bool showWatchFace;

MenuItem menuItems[MAX_MENU_ITEMS];
int itemCount = 0;
int selectedItem = 0;
int currentScreen = 0;

// =============================================
// MODULE MANAGER CLASS IMPLEMENTATION
// =============================================

class ModuleManager {
private:
    ModuleState _currentState;
    int _activeModuleIndex;
    bool _moduleActive;
    uint32_t _moduleStartTime;
    bool _exiting;
    unsigned long _exitStartTime;
    
    wifi_mode_t _savedWiFiMode;
    String _savedSSID;
    String _savedPassword;
    bool _savedWiFiState;
    uint8_t _savedWiFiChannel;
    bool _wasBLEActive;
    bool _wasIRActive;
    
    static const unsigned long EXIT_TIMEOUT = 5000;
    
    void saveWiFiState() {
        _savedWiFiMode = WiFi.getMode();
        _savedWiFiState = (_savedWiFiMode != WIFI_OFF);
        if (_savedWiFiState) {
            if (_savedWiFiMode & WIFI_STA) {
                _savedSSID = WiFi.SSID();
                _savedPassword = WiFi.psk();
                _savedWiFiChannel = WiFi.channel();
            }
        }
    }
    
    void restoreWiFiState() {
        Serial.println("🔄 Restoring WiFi state...");
        WiFi.disconnect(true);
        delay(50);
        esp_wifi_stop();
        delay(50);
        
        if (_savedWiFiState) {
            WiFi.mode(_savedWiFiMode);
            if (_savedWiFiMode & WIFI_AP) {
                WiFi.softAP(_savedSSID.c_str(), _savedPassword.c_str(), _savedWiFiChannel);
            }
            if (_savedWiFiMode & WIFI_STA) {
                WiFi.begin(_savedSSID.c_str(), _savedPassword.c_str());
            }
        } else {
            WiFi.mode(WIFI_OFF);
        }
        delay(50);
        Serial.println("✅ WiFi state restored");
    }
    
    void forceCleanup() {
        Serial.println("⚠️ Force cleanup initiated...");
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        SPIFFS.end();
        delay(50);
        SPIFFS.begin(true);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        #ifdef BLE_ENABLED
        NimBLEDevice::deinit();
        #endif
        Serial.println("✅ Force cleanup complete");
    }
    
public:
    ModuleManager() {
        _currentState = MODULE_UNINITIALIZED;
        _activeModuleIndex = -1;
        _moduleActive = false;
        _moduleStartTime = 0;
        _exiting = false;
        _exitStartTime = 0;
        _savedWiFiMode = WIFI_OFF;
        _savedWiFiState = false;
        _savedWiFiChannel = 6;
        _wasBLEActive = false;
        _wasIRActive = false;
    }
    
    bool enterModule(int index) {
        if (index < 0 || index >= itemCount) return false;
        if (menuItems[index].setupFunc == nullptr) return false;
        
        saveWiFiState();
        _activeModuleIndex = index;
        _moduleActive = true;
        _moduleStartTime = millis();
        currentScreen = 1;
        _currentState = MODULE_INITIALIZED;
        _exiting = false;
        
        menuItems[index].setupFunc();
        menuItems[index].isRunning = true;
        menuItems[index].entryTime = millis();
        _currentState = MODULE_RUNNING;
        
        Serial.printf("✅ Module entered: %s\n", menuItems[index].name);
        return true;
    }
    
    void exitModule() {
        if (!_moduleActive || _exiting) return;
        _exiting = true;
        _exitStartTime = millis();
        int index = _activeModuleIndex;
        
        Serial.printf("🧹 Exiting module: %s\n", menuItems[index].name);
        
        if (index >= 0 && index < itemCount) {
            _currentState = MODULE_STOPPING;
            if (menuItems[index].cleanupFunc != nullptr) {
                menuItems[index].cleanupFunc();
                Serial.printf("  ✅ Module cleanup done\n");
            } else {
                Serial.printf("  ⚠️ No cleanup function\n");
            }
            menuItems[index].isRunning = false;
            menuItems[index].exitTime = millis();
        }
        
        performSystemCleanup();
        restoreWiFiState();
        
        _moduleActive = false;
        _activeModuleIndex = -1;
        currentScreen = 0;
        _currentState = MODULE_UNINITIALIZED;
        _exiting = false;
        
        showWatchFace = true;
        extern void drawWatchFace();
        drawWatchFace();
        Serial.println("✅ Module exit complete - Returned to Watch Face");
    }
    
    void performSystemCleanup() {
        Serial.println("  🔄 System cleanup...");
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        WiFi.mode(WIFI_OFF);
        delay(50);
        SPIFFS.end();
        delay(50);
        if (!SPIFFS.begin(true)) {
            Serial.println("  ⚠️ SPIFFS restart failed");
        }
        #ifdef BLE_ENABLED
        if (NimBLEDevice::getInitialized()) {
            NimBLEDevice::deinit();
            Serial.println("  ✅ BLE deinitialized");
        }
        #endif
        Serial.println("  ✅ System cleanup done");
    }
    
    void emergencyExit() {
        Serial.println("🚨 EMERGENCY EXIT!");
        forceCleanup();
        _moduleActive = false;
        _activeModuleIndex = -1;
        currentScreen = 0;
        _currentState = MODULE_UNINITIALIZED;
        _exiting = false;
        extern void drawWatchFace();
        drawWatchFace();
    }
    
    bool isModuleActive() { return _moduleActive; }
    bool isExiting() { return _exiting; }
    int getActiveModuleIndex() { return _activeModuleIndex; }
    uint32_t getModuleUptime() { 
        return _moduleActive ? (millis() - _moduleStartTime) : 0; 
    }
    ModuleState getState() { return _currentState; }
};

// Global module manager instance
ModuleManager moduleManager;

// ============================================================
// BRUCE ICON DRAWING FUNCTIONS
// ============================================================

void drawBruceWifi(int x, int y) {
    u8g2.drawDisc(x + 8, y + 14, 2);
    for (int r = 6; r <= 14; r += 4) {
        for (float a = -2.0; a <= 2.0; a += 0.1) {
            int x1 = x + 8 + cos(a) * r;
            int y1 = y + 14 + sin(a) * r;
            u8g2.drawPixel(x1, y1);
        }
    }
}

void drawBruceScan(int x, int y) {
    u8g2.drawCircle(x + 8, y + 8, 4);
    u8g2.drawLine(x + 11, y + 11, x + 16, y + 16);
    u8g2.drawLine(x + 5, y + 8, x + 9, y + 8);
    u8g2.drawLine(x + 8, y + 5, x + 8, y + 9);
}

void drawBruceLightning(int x, int y) {
    u8g2.drawTriangle(x + 8, y + 2, x + 3, y + 10, x + 7, y + 10);
    u8g2.drawTriangle(x + 7, y + 8, x + 13, y + 6, x + 7, y + 16);
}

void drawBruceBLE(int x, int y) {
    u8g2.drawLine(x + 8, y + 2, x + 8, y + 16);
    u8g2.drawLine(x + 8, y + 2, x + 14, y + 7);
    u8g2.drawLine(x + 14, y + 7, x + 8, y + 11);
    u8g2.drawLine(x + 8, y + 11, x + 14, y + 15);
    u8g2.drawLine(x + 14, y + 15, x + 8, y + 16);
    u8g2.drawLine(x + 8, y + 9, x + 3, y + 5);
    u8g2.drawLine(x + 8, y + 9, x + 3, y + 13);
}

void drawBruceIR(int x, int y) {
    u8g2.drawCircle(x + 8, y + 9, 3);
    for (int r = 6; r <= 14; r += 4) {
        for (float a = -1.8; a <= 1.8; a += 0.1) {
            int x1 = x + 8 + cos(a) * r;
            int y1 = y + 9 + sin(a) * r;
            u8g2.drawPixel(x1, y1);
        }
    }
}

void drawBruceGame(int x, int y) {
    u8g2.drawRFrame(x + 2, y + 4, 20, 12, 4);
    u8g2.drawDisc(x + 10, y + 8, 1);
    u8g2.drawDisc(x + 14, y + 12, 1);
    u8g2.drawDisc(x + 18, y + 10, 1);
}

void drawBruceKey(int x, int y) {
    u8g2.drawCircle(x + 6, y + 6, 4);
    u8g2.drawLine(x + 9, y + 9, x + 16, y + 16);
    u8g2.drawLine(x + 13, y + 13, x + 15, y + 11);
    u8g2.drawLine(x + 14, y + 15, x + 16, y + 13);
}

void drawBruceFile(int x, int y) {
    u8g2.drawRFrame(x + 3, y + 1, 15, 17, 2);
    u8g2.drawLine(x + 13, y + 1, x + 18, y + 6);
    u8g2.drawLine(x + 13, y + 1, x + 13, y + 6);
    u8g2.drawLine(x + 13, y + 6, x + 18, y + 6);
    u8g2.drawLine(x + 5, y + 10, x + 14, y + 10);
    u8g2.drawLine(x + 5, y + 14, x + 14, y + 14);
}

void drawBruceSettings(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 4);
    u8g2.drawCircle(x + 9, y + 9, 8);
    for (int i = 0; i < 8; i++) {
        float a = i * 3.14159 / 4.0;
        int x1 = x + 9 + cos(a) * 7;
        int y1 = y + 9 + sin(a) * 7;
        int x2 = x + 9 + cos(a) * 10;
        int y2 = y + 9 + sin(a) * 10;
        u8g2.drawLine(x1, y1, x2, y2);
    }
}

void drawBruceInfo(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 8);
    u8g2.drawDisc(x + 9, y + 5, 1);
    u8g2.drawLine(x + 9, y + 8, x + 9, y + 13);
}

void drawBrucePhishing(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 8);
    u8g2.drawLine(x + 4, y + 4, x + 14, y + 14);
    u8g2.drawLine(x + 14, y + 4, x + 4, y + 14);
    u8g2.drawDisc(x + 9, y + 9, 2);
}

void drawBruceChat(int x, int y) {
    u8g2.drawRFrame(x + 2, y + 3, 18, 13, 4);
    u8g2.drawLine(x + 6, y + 13, x + 8, y + 16);
    u8g2.drawLine(x + 8, y + 16, x + 10, y + 13);
    u8g2.drawLine(x + 4, y + 6, x + 14, y + 6);
    u8g2.drawLine(x + 4, y + 10, x + 11, y + 10);
}

void drawBruceBook(int x, int y) {
    u8g2.drawRFrame(x + 2, y + 2, 18, 14, 2);
    u8g2.drawLine(x + 7, y + 4, x + 7, y + 14);
    u8g2.drawLine(x + 13, y + 4, x + 13, y + 14);
    u8g2.drawLine(x + 4, y + 8, x + 6, y + 8);
    u8g2.drawLine(x + 14, y + 8, x + 16, y + 8);
}

void drawBruceWatch(int x, int y) {
    u8g2.drawRFrame(x + 4, y + 4, 14, 14, 4);
    u8g2.drawCircle(x + 11, y + 11, 4);
    u8g2.drawLine(x + 11, y + 8, x + 11, y + 11);
    u8g2.drawLine(x + 11, y + 11, x + 13, y + 13);
}

void drawBruceRogue(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 7);
    u8g2.drawLine(x + 4, y + 4, x + 14, y + 14);
    u8g2.drawLine(x + 14, y + 4, x + 4, y + 14);
    u8g2.drawDisc(x + 9, y + 9, 2);
}

void drawBruceNetwork(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 7);
    u8g2.drawLine(x + 2, y + 9, x + 16, y + 9);
    u8g2.drawLine(x + 9, y + 2, x + 9, y + 16);
    u8g2.drawDisc(x + 9, y + 9, 2);
}

void drawBrucePacket(int x, int y) {
    u8g2.drawRFrame(x + 2, y + 3, 18, 13, 2);
    u8g2.drawLine(x + 4, y + 6, x + 16, y + 6);
    u8g2.drawLine(x + 4, y + 10, x + 13, y + 10);
    u8g2.drawLine(x + 4, y + 14, x + 16, y + 14);
}

void drawBruceBeacon(int x, int y) {
    u8g2.drawDisc(x + 9, y + 14, 2);
    for (int r = 5; r <= 14; r += 4) {
        for (float a = -2.2; a <= 2.2; a += 0.1) {
            int x1 = x + 9 + cos(a) * r;
            int y1 = y + 14 + sin(a) * r;
            u8g2.drawPixel(x1, y1);
        }
    }
}

void drawBruceHandshake(int x, int y) {
    u8g2.drawLine(x + 2, y + 14, x + 7, y + 6);
    u8g2.drawLine(x + 7, y + 6, x + 12, y + 14);
    u8g2.drawLine(x + 12, y + 14, x + 17, y + 6);
    u8g2.drawLine(x + 17, y + 6, x + 22, y + 14);
    u8g2.drawDisc(x + 4, y + 14, 2);
    u8g2.drawDisc(x + 9, y + 14, 2);
    u8g2.drawDisc(x + 14, y + 14, 2);
    u8g2.drawDisc(x + 19, y + 14, 2);
}

void drawBruceEvil(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 8);
    u8g2.drawDisc(x + 6, y + 7, 2);
    u8g2.drawDisc(x + 12, y + 7, 2);
    u8g2.drawLine(x + 4, y + 14, x + 14, y + 14);
    u8g2.drawLine(x + 7, y + 12, x + 11, y + 12);
}

void drawBruceBrute(int x, int y) {
    u8g2.drawRFrame(x + 4, y + 6, 12, 10, 3);
    u8g2.drawArc(x + 10, y + 6, 4, 200, 340);
    u8g2.drawDisc(x + 10, y + 11, 2);
}

void drawBruceBLETransfer(int x, int y) {
    u8g2.drawLine(x + 8, y + 2, x + 8, y + 16);
    u8g2.drawLine(x + 8, y + 2, x + 14, y + 7);
    u8g2.drawLine(x + 14, y + 7, x + 8, y + 11);
    u8g2.drawLine(x + 8, y + 11, x + 14, y + 15);
    u8g2.drawLine(x + 14, y + 15, x + 8, y + 16);
    u8g2.drawLine(x + 2, y + 9, x + 6, y + 9);
    u8g2.drawLine(x + 10, y + 9, x + 14, y + 9);
    u8g2.drawTriangle(x + 2, y + 7, x + 2, y + 11, x + 6, y + 9);
    u8g2.drawTriangle(x + 14, y + 7, x + 14, y + 11, x + 10, y + 9);
}

void drawBruceWiFiMan(int x, int y) {
    u8g2.drawDisc(x + 9, y + 14, 2);
    for (int r = 5; r <= 13; r += 4) {
        for (float a = -2.2; a <= 2.2; a += 0.1) {
            int x1 = x + 9 + cos(a) * r;
            int y1 = y + 14 + sin(a) * r;
            u8g2.drawPixel(x1, y1);
        }
    }
    u8g2.drawRFrame(x + 3, y + 2, 12, 8, 2);
    u8g2.drawLine(x + 5, y + 6, x + 13, y + 6);
}

void drawBruceMarauder(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 8);
    u8g2.drawDisc(x + 6, y + 7, 2);
    u8g2.drawDisc(x + 12, y + 7, 2);
    u8g2.drawLine(x + 4, y + 14, x + 14, y + 14);
    u8g2.drawLine(x + 9, y + 2, x + 9, y + 4);
    u8g2.drawLine(x + 7, y + 3, x + 11, y + 3);
}

void drawBruceCredential(int x, int y) {
    u8g2.drawCircle(x + 6, y + 6, 4);
    u8g2.drawLine(x + 9, y + 9, x + 16, y + 16);
    u8g2.drawLine(x + 13, y + 13, x + 15, y + 11);
    u8g2.drawLine(x + 14, y + 15, x + 16, y + 13);
    u8g2.drawDisc(x + 18, y + 12, 2);
}

void drawBruceFolder(int x, int y) {
    u8g2.drawRFrame(x + 2, y + 5, 18, 12, 2);
    u8g2.drawLine(x + 4, y + 5, x + 6, y + 3);
    u8g2.drawLine(x + 6, y + 3, x + 10, y + 3);
    u8g2.drawLine(x + 10, y + 3, x + 12, y + 5);
    u8g2.drawLine(x + 4, y + 10, x + 16, y + 10);
}

void drawBruceMPU(int x, int y) {
    u8g2.drawFrame(x + 3, y + 3, 12, 12);
    u8g2.drawLine(x + 3, y + 3, x + 7, y + 1);
    u8g2.drawLine(x + 15, y + 3, x + 19, y + 1);
    u8g2.drawLine(x + 7, y + 1, x + 19, y + 1);
    u8g2.drawLine(x + 5, y + 7, x + 17, y + 7);
}

void drawBruceAbout(int x, int y) {
    u8g2.drawCircle(x + 9, y + 9, 8);
    u8g2.setCursor(x + 6, y + 9);
    u8g2.print("?");
}

void drawBruceDeauther(int x, int y) {
    u8g2.drawTriangle(x + 8, y + 2, x + 3, y + 10, x + 7, y + 10);
    u8g2.drawTriangle(x + 7, y + 8, x + 13, y + 6, x + 7, y + 16);
    u8g2.drawCircle(x + 9, y + 9, 10);
}

void drawBruceGames(int x, int y) {
    u8g2.drawFrame(x + 4, y + 6, 12, 12);
    u8g2.drawLine(x + 10, y + 2, x + 10, y + 22);
    u8g2.drawLine(x + 2, y + 12, x + 18, y + 12);
}

void drawBruceOTA(int x, int y) {
    u8g2.drawDisc(x + 4, y + 10, 4);
    u8g2.drawDisc(x + 8, y + 7, 5);
    u8g2.drawDisc(x + 12, y + 10, 4);
    u8g2.drawBox(x + 4, y + 10, 8, 4);
    u8g2.drawLine(x + 8, y + 16, x + 8, y + 21);
    u8g2.drawLine(x + 5, y + 18, x + 8, y + 21);
    u8g2.drawLine(x + 11, y + 18, x + 8, y + 21);
}

// ============================================================
// DRAW BRUCE ICON BY INDEX
// ============================================================

void drawBruceIcon(int iconIdx, int x, int y) {
    switch (iconIdx) {
        case 0: drawBruceWifi(x, y); break;
        case 1: drawBruceScan(x, y); break;
        case 2: drawBruceLightning(x, y); break;
        case 3: drawBruceBLE(x, y); break;
        case 4: drawBruceIR(x, y); break;
        case 5: drawBruceGame(x, y); break;
        case 6: drawBruceKey(x, y); break;
        case 7: drawBruceFile(x, y); break;
        case 8: drawBruceSettings(x, y); break;
        case 9: drawBruceInfo(x, y); break;
        case 10: drawBrucePhishing(x, y); break;
        case 11: drawBruceChat(x, y); break;
        case 12: drawBruceBook(x, y); break;
        case 13: drawBruceWatch(x, y); break;
        case 14: drawBruceRogue(x, y); break;
        case 15: drawBruceNetwork(x, y); break;
        case 16: drawBrucePacket(x, y); break;
        case 17: drawBruceBeacon(x, y); break;
        case 18: drawBruceHandshake(x, y); break;
        case 19: drawBruceEvil(x, y); break;
        case 20: drawBruceBrute(x, y); break;
        case 21: drawBruceBLETransfer(x, y); break;
        case 22: drawBruceWiFiMan(x, y); break;
        case 23: drawBruceMarauder(x, y); break;
        case 24: drawBruceCredential(x, y); break;
        case 25: drawBruceFolder(x, y); break;
        case 26: drawBruceMPU(x, y); break;
        case 27: drawBruceAbout(x, y); break;
        case 28: drawBruceDeauther(x, y); break;
        case 29: drawBruceGames(x, y); break;
        case 30: drawBruceOTA(x, y); break;
        default: drawBruceWifi(x, y); break;
    }
}

// ============================================================
// DRAW FUNCTIONS
// ============================================================

void drawRoundedBox(int x, int y, int w, int h, int r) {
    if (w < 2 * r) r = w / 2;
    if (h < 2 * r) r = h / 2;
    
    u8g2.drawHLine(x + r, y, w - 2 * r);
    u8g2.drawHLine(x + r, y + h - 1, w - 2 * r);
    u8g2.drawVLine(x, y + r, h - 2 * r);
    u8g2.drawVLine(x + w - 1, y + r, h - 2 * r);
    
    u8g2.drawCircle(x + r, y + r, r, U8G2_DRAW_UPPER_LEFT);
    u8g2.drawCircle(x + w - r - 1, y + r, r, U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawCircle(x + r, y + h - r - 1, r, U8G2_DRAW_LOWER_LEFT);
    u8g2.drawCircle(x + w - r - 1, y + h - r - 1, r, U8G2_DRAW_LOWER_RIGHT);
}

void drawFilledRoundedBox(int x, int y, int w, int h, int r) {
    if (w < 2 * r) r = w / 2;
    if (h < 2 * r) r = h / 2;
    
    u8g2.drawBox(x + r, y, w - 2 * r, h);
    u8g2.drawBox(x, y + r, w, h - 2 * r);
    
    u8g2.drawDisc(x + r, y + r, r, U8G2_DRAW_ALL);
    u8g2.drawDisc(x + w - r - 1, y + r, r, U8G2_DRAW_ALL);
    u8g2.drawDisc(x + r, y + h - r - 1, r, U8G2_DRAW_ALL);
    u8g2.drawDisc(x + w - r - 1, y + h - r - 1, r, U8G2_DRAW_ALL);
}

void drawPreviousItem() {
    int idx = (selectedItem - 1 + itemCount) % itemCount;
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(10, 10);
    u8g2.print("▲ ");
    u8g2.print(menuItems[idx].name);
}

void drawNextItem() {
    int idx = (selectedItem + 1) % itemCount;
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(10, 62);
    u8g2.print("▼ ");
    u8g2.print(menuItems[idx].name);
}

void drawCurrentItem() {
    u8g2.setDrawColor(1);
    u8g2.drawRFrame(2, 12, 124, 42, 5);
    
    // TOP BAR: Time on left, Battery on right
    u8g2.setFont(u8g2_font_5x7_tf);
    
    // Time display (left)
    u8g2.setCursor(10, 22);
    u8g2.print(currentTimeStr.c_str());
    
    // Module name (center)
    u8g2.setCursor(75, 22);
    if (currentScreen == 1 && moduleManager.isModuleActive()) {
        u8g2.print(menuItems[selectedItem].name);
    } else {
        u8g2.print("Multi-Tool");
    }
    
    // Battery (right)
    if (batteryLevel > 0) {
        u8g2.setCursor(113, 22);
        u8g2.print(batteryLevel);
        u8g2.print("%");
    }
    
    u8g2.drawLine(8, 26, 120, 26);
    
    // ICON (Left side)
    drawBruceIcon(menuItems[selectedItem].iconIndex, 10, 32);
    
    // MODULE NAME - Centered Large Font
    u8g2.setFont(u8g2_font_7x13_tf);
    const char* moduleName = menuItems[selectedItem].name;
    int nameWidth = u8g2.getStrWidth(moduleName);
    int nameX = (128 - nameWidth) / 2;
    if (nameX < 42) nameX = 42;
    
    u8g2.setCursor(nameX, 42);
    u8g2.print(moduleName);
    
    // SUBTEXT
    u8g2.setFont(u8g2_font_4x6_tf);
    if (menuItems[selectedItem].isRunning) {
        const char* status = "🟢 Running";
        int statusWidth = u8g2.getStrWidth(status);
        u8g2.setCursor((128 - statusWidth) / 2, 50);
        u8g2.print(status);
    } else {
        const char* status = "Press SELECT";
        int statusWidth = u8g2.getStrWidth(status);
        u8g2.setCursor((128 - statusWidth) / 2, 50);
        u8g2.print(status);
    }
}

// ============================================================
// DRAW MAIN MENU
// ============================================================

void drawMenu() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    
    if (WiFi.getMode() != WIFI_OFF) {
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(0, 63);
        u8g2.print("📶");
    }
    
    drawPreviousItem();
    drawCurrentItem();
    drawNextItem();
    
    u8g2.sendBuffer();
}

// ============================================================
// SCROLLING MENU - Used by modules
// ============================================================

void drawScrollingMenu(const char** items, int itemCount, int selected, int maxVisible, const char* title, int iconIdx) {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    
    drawBruceIcon(iconIdx, 2, 2);
    
    u8g2.setFont(u8g2_font_7x13_tf);
    u8g2.setCursor(20, 12);
    u8g2.print(title);
    u8g2.drawLine(0, 15, 127, 15);
    
    int boxX = 4;
    int boxY = 20;
    int boxW = 120;
    int boxH = 38;
    int cornerRadius = 8;
    
    drawRoundedBox(boxX, boxY, boxW, boxH, cornerRadius);
    
    if (selected < 0) selected = 0;
    if (selected >= itemCount) selected = itemCount - 1;
    
    int scrollOffset = 0;
    if (itemCount > maxVisible) {
        scrollOffset = selected - 1;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset + maxVisible > itemCount) {
            scrollOffset = itemCount - maxVisible;
        }
    }
    
    int startItem = scrollOffset;
    int endItem = startItem + maxVisible;
    if (endItem > itemCount) endItem = itemCount;
    
    int itemHeight = 9;
    int totalItems = endItem - startItem;
    int totalHeight = totalItems * itemHeight;
    int startY = boxY + (boxH - totalHeight) / 2 + 7;
    
    for (int i = startItem; i < endItem; i++) {
        int posInList = i - startItem;
        int yPos = startY + posInList * itemHeight;
        int textWidth = u8g2.getStrWidth(items[i]);
        int textX = boxX + (boxW - textWidth) / 2;
        
        if (i == selected) {
            int itemBoxX = boxX + 4;
            int itemBoxW = boxW - 8;
            int itemBoxY = yPos - 7;
            int itemBoxH = 9;
            
            drawFilledRoundedBox(itemBoxX, itemBoxY, itemBoxW, itemBoxH, 4);
            
            u8g2.setDrawColor(0);
            u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.setCursor(textX, yPos);
            u8g2.print(items[i]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.setCursor(textX, yPos);
            u8g2.print(items[i]);
        }
    }
    
    if (scrollOffset > 0) {
        u8g2.setCursor(boxX + 54, boxY + 4);
        u8g2.print("▲");
    }
    if (scrollOffset + maxVisible < itemCount) {
        u8g2.setCursor(boxX + 54, boxY + boxH - 4);
        u8g2.print("▼");
    }
    
    u8g2.sendBuffer();
}

// ============================================================
// MENU ITEM FUNCTIONS
// ============================================================

void addMenuItem(const char* name, int iconIdx, void (*setup)(), void (*loop)(), void (*cleanup)()) {
    if (itemCount < MAX_MENU_ITEMS) {
        strncpy(menuItems[itemCount].name, name, MAX_ITEM_LENGTH - 1);
        menuItems[itemCount].name[MAX_ITEM_LENGTH - 1] = '\0';
        menuItems[itemCount].iconIndex = iconIdx;
        menuItems[itemCount].setupFunc = setup;
        menuItems[itemCount].loopFunc = loop;
        menuItems[itemCount].cleanupFunc = cleanup;
        menuItems[itemCount].pauseFunc = nullptr;
        menuItems[itemCount].resumeFunc = nullptr;
        menuItems[itemCount].state = MODULE_UNINITIALIZED;
        menuItems[itemCount].isRunning = false;
        menuItems[itemCount].needsCleanup = false;
        itemCount++;
    }
}

// ============================================================
// INIT MENU - 19 MODULES WITH BRUCE ICON INDEXES
// ============================================================

void initMenu() {
    // WiFi Attack Modules (7 modules)
    addMenuItem("Chat", 11, chatSetup, chatLoop, chatCleanup);
    addMenuItem("Rogue AP", 10, rogueAPSetup, rogueAPLoop, rogueAPCleanup);
    addMenuItem("Evil Portal", 19, evilPortalSetup, evilPortalLoop, evilPortalCleanup);
    addMenuItem("Deauther", 28, deautherSetup, deautherLoop, deautherCleanup);
    addMenuItem("Beacon Flood", 17, beaconFloodSetup, beaconFloodLoop, beaconFloodCleanup);
    addMenuItem("Handshake", 18, handshakeCaptureSetup, handshakeCaptureLoop, handshakeCleanup);
    addMenuItem("WiFi Brute", 20, wifiBruteSetup, wifiBruteLoop, wifiBruteCleanup);
    
    // Network Tools (3 modules)
    addMenuItem("WiFi Scan", 0, wifiScannerSetup, wifiScannerLoop, wifiScannerCleanup);
    addMenuItem("Network Map", 15, networkMapperSetup, networkMapperLoop, networkMapperCleanup);
    addMenuItem("Packet Monitor", 16, packetMonitorSetup, packetMonitorLoop, packetMonitorCleanup);
    
    // IR Tools (1 module)
    addMenuItem("IR Tools", 4, irSetup, irLoop, irCleanup);
    
    // BLE Modules (2 modules)
    addMenuItem("BLE Marauder", 23, marauderBleSetup, marauderBleLoop, marauderBleCleanup);
    addMenuItem("BLE Remote", 3, bleRemoteSetup, bleRemoteLoop, bleRemoteCleanup);
    
    // OTA Update (1 module)
    addMenuItem("OTA Update", 30, otaSetup, otaLoop, otaCleanup);
    
    // ✅ E-READER MODULE (1 module)
    addMenuItem("E-Reader", 12, ereaderSetup, ereaderLoop, ereaderCleanup);  // bruce_book icon
    
    // Utilities (4 modules)
    addMenuItem("Credentials", 24, credentialsSetup, credentialsLoop, nullptr);
    addMenuItem("File Manager", 25, fileManagerSetup, fileManagerLoop, fileManagerCleanup);
    addMenuItem("Settings", 8, settingsSetup, settingsLoop, nullptr);
    addMenuItem("About", 27, aboutSetup, aboutLoop, nullptr);
    
    Serial.printf("✅ Total modules: %d\n", itemCount);
}

// ============================================================
// MODULE ENTRY/EXIT
// ============================================================

void enterModule(int index) {
    moduleManager.enterModule(index);
}

void exitModule() {
    moduleManager.exitModule();
}

void emergencyExitModule() {
    moduleManager.emergencyExit();
}