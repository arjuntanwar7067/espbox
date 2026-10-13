#include "wifi_scanner.h"
#include "config.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include <WiFi.h>
#include <U8g2lib.h>
#include "pins.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

String wifiSSID[20], wifiBSSID[20];
int wifiCh[20], wifiRSSI[20], wifiCount = 0;
int wifiDetailIndex = 0;
int wifiMenuState = 0;
unsigned long wifiLastScan = 0;
bool wifiScanning = false;

int detectChannelFromBSSID(String bssid) {
    if (bssid.indexOf(":00:") != -1) return 1;
    if (bssid.indexOf(":01:") != -1) return 2;
    if (bssid.indexOf(":02:") != -1) return 3;
    if (bssid.indexOf(":03:") != -1) return 4;
    if (bssid.indexOf(":04:") != -1) return 5;
    if (bssid.indexOf(":05:") != -1) return 6;
    if (bssid.indexOf(":06:") != -1) return 7;
    if (bssid.indexOf(":07:") != -1) return 8;
    if (bssid.indexOf(":08:") != -1) return 9;
    if (bssid.indexOf(":09:") != -1) return 10;
    if (bssid.indexOf(":0A:") != -1) return 11;
    if (bssid.indexOf(":0B:") != -1) return 12;
    if (bssid.indexOf(":0C:") != -1) return 13;
    if (bssid.indexOf(":0D:") != -1) return 14;
    return 0;
}

void wifiScannerSetup() {
    enableWiFi();
    wifiCount = 0;
    wifiMenuState = 0;
    wifiDetailIndex = 0;
    wifiScanning = false;
    wifiLastScan = 0;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_wifi);
    CenteredStr(25, "WiFi Scanner");
    CenteredStr(40, "SELECT: Scan");
    CenteredStr(55, "BACK: Exit");
    u8g2.sendBuffer();
}

void wifiScannerLoop() {
    if(isButtonPressed(BTN_BACK)) { 
        exitModule(); 
        return; 
    }
    
    if(wifiMenuState == 0) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawXBMP(56, 5, 16, 16, icon_wifi);
        CenteredStr(25, "WiFi Scanner");
        
        if (wifiScanning) {
            CenteredStr(40, "⏳ Scanning...");
        } else {
            CenteredStr(40, "SELECT: Scan");
        }
        CenteredStr(55, "BACK: Exit");
        
        if (wifiCount > 0) {
            char countStr[20];
            sprintf(countStr, "Found: %d", wifiCount);
            u8g2.setCursor(0, 63);
            u8g2.print(countStr);
        }
        
        u8g2.sendBuffer();
        
        if(isButtonPressed(BTN_SELECT) && !wifiScanning) {
            wifiScanning = true;
            u8g2.clearBuffer();
            CenteredStr(30, "Scanning...");
            u8g2.sendBuffer();
            
            WiFi.scanDelete();
            delay(100);
            WiFi.mode(WIFI_STA);
            delay(100);
            
            int n = WiFi.scanNetworks();
            wifiLastScan = millis();
            
            if(n > 0) {
                wifiCount = min(n, 20);
                for(int i = 0; i < wifiCount; i++) {
                    wifiSSID[i] = WiFi.SSID(i);
                    if(wifiSSID[i].length() == 0) wifiSSID[i] = "Hidden";
                    wifiBSSID[i] = WiFi.BSSIDstr(i);
                    
                    int ch = WiFi.channel(i);
                    if (ch == 0 || ch > 13) {
                        ch = detectChannelFromBSSID(wifiBSSID[i]);
                    }
                    if (ch == 0 || ch > 13) ch = 1;
                    
                    wifiCh[i] = ch;
                    wifiRSSI[i] = WiFi.RSSI(i);
                }
                wifiMenuState = 1;
                wifiDetailIndex = 0;
            } else {
                u8g2.clearBuffer();
                CenteredStr(30, "No Networks!");
                u8g2.sendBuffer();
                delay(1000);
                wifiMenuState = 0;
            }
            WiFi.scanDelete();
            wifiScanning = false;
        }
        return;
    }
    
    if(wifiMenuState == 1) {
        String* displayItems = new String[wifiCount];
        for(int i = 0; i < wifiCount; i++) {
            char item[40];
            int bars = map(abs(wifiRSSI[i]), 0, 100, 1, 4);
            String signal = "";
            for(int b = 0; b < bars; b++) signal += "|";
            
            sprintf(item, "%s CH%d %ddBm %s", 
                    wifiSSID[i].substring(0, 8).c_str(), 
                    wifiCh[i], 
                    wifiRSSI[i],
                    signal.c_str());
            displayItems[i] = String(item);
        }
        
        const char** items = new const char*[wifiCount];
        for(int i = 0; i < wifiCount; i++) items[i] = displayItems[i].c_str();
        
        drawScrollingMenu(items, wifiCount, wifiDetailIndex, 5, "Networks", 2);
        
        if(isButtonPressed(BTN_UP)) {
            wifiDetailIndex = (wifiDetailIndex - 1 + wifiCount) % wifiCount;
        }
        if(isButtonPressed(BTN_DOWN)) {
            wifiDetailIndex = (wifiDetailIndex + 1) % wifiCount;
        }
        if(isButtonPressed(BTN_SELECT)) {
            wifiMenuState = 2;
        }
        if(isButtonPressed(BTN_BACK)) {
            wifiMenuState = 0;
        }
        
        delete[] displayItems;
        delete[] items;
        return;
    }
    
    if(wifiMenuState == 2) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawXBMP(56, 0, 16, 16, icon_wifi);
        u8g2.drawLine(0, 12, 128, 12);
        
        u8g2.setCursor(0, 22);
        u8g2.print("SSID: "); 
        if (wifiSSID[wifiDetailIndex].length() > 20) {
            u8g2.print(wifiSSID[wifiDetailIndex].substring(0, 18).c_str());
        } else {
            u8g2.print(wifiSSID[wifiDetailIndex].c_str());
        }
        
        u8g2.setCursor(0, 32);
        u8g2.print("BSSID: "); 
        u8g2.print(wifiBSSID[wifiDetailIndex].c_str());
        
        u8g2.setCursor(0, 42);
        u8g2.print("Channel: "); 
        u8g2.print(wifiCh[wifiDetailIndex]);
        
        u8g2.setCursor(0, 52);
        u8g2.print("RSSI: "); 
        u8g2.print(wifiRSSI[wifiDetailIndex]); 
        u8g2.print(" dBm");
        
        int bars = map(abs(wifiRSSI[wifiDetailIndex]), 0, 100, 1, 5);
        u8g2.setCursor(85, 52);
        for(int i = 0; i < bars; i++) u8g2.print("█");
        for(int i = bars; i < 5; i++) u8g2.print("░");
        
        u8g2.setCursor(0, 63);
        u8g2.print("BACK: Back  SEL:Scan");
        u8g2.sendBuffer();
        
        if(isButtonPressed(BTN_BACK)) {
            wifiMenuState = 1;
        }
        if(isButtonPressed(BTN_SELECT)) {
            wifiMenuState = 0;
            wifiScanning = true;
        }
    }
    delay(20);
}

// =============================================
// WIFI SCANNER CLEANUP
// =============================================
void wifiScannerCleanup() {
    Serial.println("🧹 WiFi Scanner cleanup...");
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    wifiCount = 0;
    wifiMenuState = 0;
    wifiDetailIndex = 0;
    wifiScanning = false;
    Serial.println("✅ WiFi Scanner cleanup complete");
}