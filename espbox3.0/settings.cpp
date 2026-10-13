#include "settings.h"
#include "config.h"
#include "utils.h"
#include "icons.h"
#include "credentials.h"
#include "menu.h"
#include "ota.h"
#include <EEPROM.h>
#include <U8g2lib.h>
#include "pins.h"
#include <WiFi.h>
#include <time.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern uint8_t oledBrightness;
extern int credCount;

// =============================================
// EXTERNAL TIME VARIABLES
// =============================================
extern uint64_t rtc_unixTime;
extern bool rtc_timeSet;
extern bool rtc_timeSynced;
extern int watchHours, watchMinutes, watchSeconds;
extern int watchDay, watchMonth, watchYear;
extern int watchWeekDay;
extern String currentTimeStr;
extern String currentWeekdayStr;
extern String currentHours, currentMinutes, currentSeconds;

// =============================================
// DEEP SLEEP CONTROL
// =============================================
extern bool deepSleepEnabled;

// =============================================
// SETTINGS ENUMS
// =============================================
enum SettingMenu {
    SETTINGS_MAIN,
    SETTINGS_BRIGHTNESS,
    SETTINGS_WIFI,
    SETTINGS_SYSTEM,
    SETTINGS_STORAGE,
    SETTINGS_FACTORY_RESET,
    SETTINGS_TIME,
    SETTINGS_TIMEZONE,
    SETTINGS_TIME_MANUAL,
    SETTINGS_TIME_NTP
};

int settingsState = SETTINGS_MAIN;
int settingsSelection = 0;

const char* settingsItems[] = {
    "Brightness",
    "WiFi Settings",
    "Time Settings",
    "Deep Sleep",
    "System Info",
    "Storage Info",
    "Factory Reset",
    "< Back"
};
const int settingsCount = 8;

const char* wifiItems[] = {
    "Enable WiFi",
    "Disable WiFi",
    "Scan Networks",
    "Set WiFi Credentials",
    "Clear Credentials",
    "< Back"
};
const int wifiCount = 6;

// =============================================
// TIME SETTINGS STATE
// =============================================
enum TimeSettingState {
    TIME_MAIN,
    TIME_MANUAL,
    TIME_NTP,
    TIME_TIMEZONE,
    TIME_FORMAT
};

int timeState = TIME_MAIN;
int timeSelection = 0;
int timeEditIndex = 0;
bool timeEditing = false;

int editHour = 0;
int editMinute = 0;
int editSecond = 0;
int editDay = 1;
int editMonth = 1;
int editYear = 2024;

int timezoneOffset = 19800;
const char* timezoneNames[] = {
    "UTC-12:00", "UTC-11:00", "UTC-10:00", "UTC-09:00", "UTC-08:00",
    "UTC-07:00", "UTC-06:00", "UTC-05:00", "UTC-04:00", "UTC-03:00",
    "UTC-02:00", "UTC-01:00", "UTC+00:00", "UTC+01:00", "UTC+02:00",
    "UTC+03:00", "UTC+03:30", "UTC+04:00", "UTC+04:30", "UTC+05:00",
    "UTC+05:30", "UTC+05:45", "UTC+06:00", "UTC+06:30", "UTC+07:00",
    "UTC+08:00", "UTC+08:45", "UTC+09:00", "UTC+09:30", "UTC+10:00",
    "UTC+10:30", "UTC+11:00", "UTC+12:00", "UTC+13:00", "UTC+14:00"
};
const int timezoneCount = 35;
int timezoneIndex = 20;

const char* ntpServers[] = {
    "time.google.com", "pool.ntp.org", "0.pool.ntp.org",
    "1.pool.ntp.org", "2.pool.ntp.org", "3.pool.ntp.org",
    "time.windows.com", "time.apple.com"
};
const int ntpServerCount = 8;
int selectedNtpServer = 0;
bool ntpSyncing = false;
bool ntpSyncSuccess = false;
bool use12HourFormat = false;

// =============================================
// FORWARD DECLARATIONS
// =============================================
void drawSettingsMenu();
void drawWifiSettings();
void drawSystemInfo();
void drawStorageInfo();
void drawFactoryReset();
void drawBrightnessDisplay();
void drawTimeMainMenu();
void drawTimeManual();
void drawTimeNTP();
void drawTimeTimezone();
void drawTimeFormat();
void handleMainMenu();
void handleBrightness();
void handleWifiMenu();
void handleSystemInfo();
void handleStorageInfo();
void handleFactoryReset();
void handleTimeMain();
void handleTimeManual();
void handleTimeNTP();
void handleTimeTimezone();
void handleTimeFormat();
void scanWiFiNetworks();
void handleSetWiFiCredentials();
void saveTimeToRTC();
void loadTimezoneFromEEPROM();
void saveTimezoneToEEPROM();
void loadTimeFormatFromEEPROM();
void saveTimeFormatToEEPROM();
bool syncTimeWithNTP(const char* server = nullptr);

// =============================================
// TIME HELPER FUNCTIONS
// =============================================

String getTimezoneString(int offset) {
    int hours = offset / 3600;
    int minutes = (offset % 3600) / 60;
    char buf[10];
    if (offset >= 0) {
        snprintf(buf, sizeof(buf), "UTC+%02d:%02d", hours, minutes);
    } else {
        snprintf(buf, sizeof(buf), "UTC-%02d:%02d", -hours, -minutes);
    }
    return String(buf);
}

int getTimezoneOffsetFromIndex(int index) {
    int offsets[] = {
        -43200, -39600, -36000, -32400, -28800, -25200, -21600, -18000,
        -14400, -10800, -7200,  -3600,   0,      3600,   7200,   10800,
        12600,  14400,  16200,  18000,  19800,  20700,  21600,  23400,
        25200,  28800,  31500,  32400,  34200,  36000,  37800,  39600,
        43200,  46800,  50400
    };
    if (index >= 0 && index < 35) return offsets[index];
    return 19800;
}

void applyTimezone() {
    timezoneOffset = getTimezoneOffsetFromIndex(timezoneIndex);
    if (rtc_timeSet) {
        saveTimeToRTC();
    }
}

void saveTimeToRTC() {
    struct tm timeinfo;
    timeinfo.tm_hour = editHour;
    timeinfo.tm_min = editMinute;
    timeinfo.tm_sec = editSecond;
    timeinfo.tm_mday = editDay;
    timeinfo.tm_mon = editMonth - 1;
    timeinfo.tm_year = editYear - 1900;
    timeinfo.tm_isdst = -1;
    
    time_t utcTime = mktime(&timeinfo) - timezoneOffset;
    rtc_unixTime = (uint64_t)utcTime;
    rtc_timeSet = true;
    rtc_timeSynced = false;
    
    extern void updateWatchTime();
    updateWatchTime();
    
    Serial.println("✅ Time saved to RTC");
}

void loadTimezoneFromEEPROM() {
    EEPROM.begin(512);
    int saved = EEPROM.read(50);
    EEPROM.end();
    if (saved >= 0 && saved < timezoneCount) {
        timezoneIndex = saved;
        timezoneOffset = getTimezoneOffsetFromIndex(saved);
    }
}

void saveTimezoneToEEPROM() {
    EEPROM.begin(512);
    EEPROM.write(50, timezoneIndex);
    EEPROM.commit();
    EEPROM.end();
}

void loadTimeFormatFromEEPROM() {
    EEPROM.begin(512);
    use12HourFormat = EEPROM.read(51) == 1;
    EEPROM.end();
}

void saveTimeFormatToEEPROM() {
    EEPROM.begin(512);
    EEPROM.write(51, use12HourFormat ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}

bool syncTimeWithNTP(const char* server) {
    Serial.println("📡 Syncing time with NTP...");
    ntpSyncing = true;
    ntpSyncSuccess = false;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(20, "⏳ Syncing Time...");
    if (server) {
        CenteredStr(35, server);
    } else {
        CenteredStr(35, ntpServers[selectedNtpServer]);
    }
    u8g2.sendBuffer();
    
    bool wifiConnected = false;
    if (WiFi.status() != WL_CONNECTED) {
        if (savedWiFi.valid) {
            WiFi.mode(WIFI_STA);
            WiFi.begin(savedWiFi.ssid, savedWiFi.password);
            
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
                delay(200);
                attempts++;
                if (attempts % 5 == 0) {
                    u8g2.clearBuffer();
                    CenteredStr(20, "⏳ Connecting WiFi...");
                    char buf[30];
                    snprintf(buf, sizeof(buf), "%d/30", attempts);
                    CenteredStr(35, buf);
                    u8g2.sendBuffer();
                }
            }
        }
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        ntpSyncing = false;
        showPopupMessage("❌ No WiFi!", "Connect to WiFi first", "");
        delay(1000);
        return false;
    }
    
    const char* servers[] = {
        server ? server : ntpServers[selectedNtpServer],
        "time.google.com", "pool.ntp.org",
        "0.pool.ntp.org", "1.pool.ntp.org", "time.windows.com"
    };
    
    bool success = false;
    struct tm timeinfo;
    
    for (int i = 0; i < 6 && !success; i++) {
        if (i > 0) {
            u8g2.clearBuffer();
            CenteredStr(20, "⏳ Retrying...");
            char buf[30];
            snprintf(buf, sizeof(buf), "Server: %s", servers[i]);
            CenteredStr(35, buf);
            u8g2.sendBuffer();
            delay(1000);
        }
        
        configTime(timezoneOffset, 0, servers[i]);
        
        for (int attempt = 0; attempt < 3 && !success; attempt++) {
            if (getLocalTime(&timeinfo, 5000)) {
                time_t testTime = mktime(&timeinfo);
                if (testTime > 1577836800) {
                    success = true;
                    break;
                }
            }
            delay(500);
        }
    }
    
    if (success) {
        rtc_unixTime = (uint64_t)mktime(&timeinfo);
        rtc_timeSet = true;
        rtc_timeSynced = true;
        
        editHour = timeinfo.tm_hour;
        editMinute = timeinfo.tm_min;
        editSecond = timeinfo.tm_sec;
        editDay = timeinfo.tm_mday;
        editMonth = timeinfo.tm_mon + 1;
        editYear = timeinfo.tm_year + 1900;
        
        extern void updateWatchTime();
        updateWatchTime();
        
        showPopupMessage("✅ Time Synced!", currentTimeStr.c_str(), "");
        delay(1000);
        ntpSyncSuccess = true;
        ntpSyncing = false;
        return true;
    }
    
    ntpSyncing = false;
    ntpSyncSuccess = false;
    showPopupMessage("❌ Sync Failed!", "Check WiFi/Server", "");
    delay(1000);
    return false;
}

// =============================================
// DRAW SETTINGS MENU
// =============================================

void drawSettingsMenu() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_settings);
    CenteredStr(18, "Settings");
    u8g2.drawLine(0, 20, 128, 20);
    
    for(int i = 0; i < settingsCount; i++) {
        int y = 26 + i * 8;
        if(i == settingsSelection) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 8);
            u8g2.setDrawColor(1);
            u8g2.setCursor(5, y + 1);
            u8g2.print(">");
            u8g2.setCursor(15, y + 1);
        } else {
            u8g2.setCursor(5, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(15, y + 1);
        }
        u8g2.print(settingsItems[i]);
    }
    
    char brightStr[16];
    sprintf(brightStr, "%d%%", map(oledBrightness, 0, 255, 0, 100));
    u8g2.setCursor(100, 34);
    u8g2.print(brightStr);
    
    char sleepStr[8];
    sprintf(sleepStr, deepSleepEnabled ? "ON" : "OFF");
    u8g2.setCursor(100, 42);
    u8g2.print(sleepStr);
    
    u8g2.setCursor(0, 63);
    u8g2.print(currentTimeStr.c_str());
    u8g2.setCursor(70, 63);
    u8g2.print(rtc_timeSynced ? "✅NTP" : "⚠️Manual");
    
    u8g2.sendBuffer();
}

// =============================================
// DRAW WIFI SETTINGS
// =============================================

void drawWifiSettings() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(10, "WiFi Settings");
    u8g2.drawLine(0, 12, 128, 12);
    
    char wifiStatus[20];
    if(WiFi.getMode() == WIFI_AP) {
        sprintf(wifiStatus, "Mode: AP");
    } else if(WiFi.getMode() == WIFI_STA) {
        sprintf(wifiStatus, "Mode: STA");
    } else {
        sprintf(wifiStatus, "Mode: OFF");
    }
    u8g2.setCursor(5, 24);
    u8g2.print(wifiStatus);
    
    if (savedWiFi.valid) {
        u8g2.setCursor(5, 34);
        u8g2.print("SSID: ");
        u8g2.print(savedWiFi.ssid);
    } else {
        u8g2.setCursor(5, 34);
        u8g2.print("No credentials saved");
    }
    
    for(int i = 0; i < wifiCount; i++) {
        int y = 46 + i * 8;
        if(i == settingsSelection) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 8);
            u8g2.setDrawColor(1);
            u8g2.setCursor(5, y + 1);
            u8g2.print(">");
            u8g2.setCursor(15, y + 1);
        } else {
            u8g2.setCursor(5, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(15, y + 1);
        }
        u8g2.print(wifiItems[i]);
    }
    
    u8g2.sendBuffer();
}

// =============================================
// DRAW TIME MAIN MENU
// =============================================

void drawTimeMainMenu() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_settings);
    CenteredStr(18, "⏰ Time Settings");
    u8g2.drawLine(0, 20, 128, 20);
    
    const char* timeItems[] = {
        "Set Time (Manual)",
        "Sync via WiFi (NTP)",
        "Time Zone",
        "12/24 Hour Format",
        "< Back"
    };
    int count = 5;
    
    for(int i = 0; i < count; i++) {
        int y = 26 + i * 8;
        if(i == timeSelection) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 8);
            u8g2.setDrawColor(1);
            u8g2.setCursor(5, y + 1);
            u8g2.print(">");
            u8g2.setCursor(15, y + 1);
        } else {
            u8g2.setCursor(5, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(15, y + 1);
        }
        u8g2.print(timeItems[i]);
    }
    
    u8g2.setCursor(0, 63);
    u8g2.print(currentTimeStr.c_str());
    u8g2.setCursor(70, 63);
    u8g2.print(getTimezoneString(timezoneOffset).c_str());
    
    u8g2.sendBuffer();
}

// =============================================
// DRAW TIME MANUAL
// =============================================

void drawTimeManual() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_settings);
    CenteredStr(18, "⏰ Set Time Manually");
    u8g2.drawLine(0, 20, 128, 20);
    
    char timeStr[20];
    if (use12HourFormat) {
        int h12 = editHour % 12;
        if (h12 == 0) h12 = 12;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d %s", 
                 h12, editMinute, editSecond, 
                 editHour >= 12 ? "PM" : "AM");
    } else {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                 editHour, editMinute, editSecond);
    }
    
    int xPositions[] = {20, 44, 68};
    int x = xPositions[timeEditIndex % 3];
    u8g2.drawFrame(x - 2, 26, 18, 11);
    
    u8g2.setCursor(22, 36);
    u8g2.print(timeStr);
    
    char dateStr[30];
    snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", editDay, editMonth, editYear);
    u8g2.setCursor(22, 52);
    u8g2.print(dateStr);
    
    u8g2.setCursor(0, 63);
    u8g2.print("UP/DN:Change  SEL:Next  BACK:Save");
    u8g2.sendBuffer();
}

// =============================================
// DRAW TIME NTP
// =============================================

void drawTimeNTP() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_wifi);
    CenteredStr(18, "📡 Sync via WiFi");
    u8g2.drawLine(0, 20, 128, 20);
    
    u8g2.setCursor(2, 30);
    u8g2.print("Server: ");
    u8g2.print(ntpServers[selectedNtpServer]);
    
    u8g2.setCursor(2, 42);
    if (ntpSyncing) {
        u8g2.print("⏳ Syncing...");
    } else if (ntpSyncSuccess) {
        u8g2.print("✅ Last sync successful");
    } else {
        u8g2.print("Press SELECT to sync");
    }
    
    u8g2.setCursor(2, 54);
    u8g2.print("WiFi: ");
    if (WiFi.status() == WL_CONNECTED) {
        u8g2.print("✅ Connected");
    } else if (savedWiFi.valid) {
        u8g2.print("⚠️ Saved: ");
        u8g2.print(savedWiFi.ssid);
    } else {
        u8g2.print("❌ No WiFi saved");
    }
    
    u8g2.setCursor(0, 63);
    u8g2.print("UP/DN:Server  SEL:Sync  BACK:Exit");
    u8g2.sendBuffer();
}

// =============================================
// DRAW TIME TIMEZONE
// =============================================

void drawTimeTimezone() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_settings);
    CenteredStr(18, "🌍 Time Zone");
    u8g2.drawLine(0, 20, 128, 20);
    
    u8g2.setCursor(2, 32);
    u8g2.print("Selected:");
    u8g2.setCursor(2, 44);
    u8g2.print(timezoneNames[timezoneIndex]);
    
    u8g2.setCursor(2, 56);
    u8g2.print("Offset: ");
    u8g2.print(getTimezoneString(timezoneOffset).c_str());
    
    u8g2.setCursor(0, 63);
    u8g2.print("UP/DN:Change  SEL:Save  BACK:Exit");
    u8g2.sendBuffer();
}

// =============================================
// DRAW TIME FORMAT
// =============================================

void drawTimeFormat() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_settings);
    CenteredStr(18, "⏰ Time Format");
    u8g2.drawLine(0, 20, 128, 20);
    
    const char* formatItems[] = {
        "12-Hour Format",
        "24-Hour Format",
        "< Back"
    };
    int count = 3;
    
    for(int i = 0; i < count; i++) {
        int y = 26 + i * 10;
        if(i == timeSelection) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, 10);
            u8g2.setDrawColor(1);
            u8g2.setCursor(5, y + 1);
            u8g2.print(">");
            u8g2.setCursor(15, y + 1);
        } else {
            u8g2.setCursor(5, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(15, y + 1);
        }
        u8g2.print(formatItems[i]);
        
        if (i == 0 && !use12HourFormat) {
            u8g2.setCursor(115, y + 1);
            u8g2.print("✓");
        } else if (i == 1 && use12HourFormat) {
            u8g2.setCursor(115, y + 1);
            u8g2.print("✓");
        }
    }
    
    u8g2.sendBuffer();
}

// =============================================
// DRAW SYSTEM INFO
// =============================================

void drawSystemInfo() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(10, "System Info");
    u8g2.drawLine(0, 12, 128, 12);
    
    char buf[30];
    sprintf(buf, "Heap: %d KB", ESP.getFreeHeap() / 1024);
    u8g2.setCursor(5, 24);
    u8g2.print(buf);
    
    sprintf(buf, "Chip: %s", ESP.getChipModel());
    u8g2.setCursor(5, 36);
    u8g2.print(buf);
    
    sprintf(buf, "Core: %d", ESP.getChipCores());
    u8g2.setCursor(5, 48);
    u8g2.print(buf);
    
    sprintf(buf, "Flash: %d MB", ESP.getFlashChipSize() / (1024 * 1024));
    u8g2.setCursor(5, 60);
    u8g2.print(buf);
    
    u8g2.sendBuffer();
}

// =============================================
// DRAW STORAGE INFO
// =============================================

void drawStorageInfo() {
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

// =============================================
// DRAW BRIGHTNESS DISPLAY
// =============================================

void drawBrightnessDisplay() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(20, "Brightness");
    
    int percent = map(oledBrightness, 0, 255, 0, 100);
    char brightStr[16];
    sprintf(brightStr, "%d%%", percent);
    CenteredStr(35, brightStr);
    
    int barWidth = map(oledBrightness, 0, 255, 0, 100);
    u8g2.drawFrame(14, 45, 100, 8);
    u8g2.drawBox(16, 47, barWidth, 4);
    
    CenteredStr(58, "UP/DN adjust  BACK:Save");
    u8g2.sendBuffer();
}

// =============================================
// DRAW FACTORY RESET
// =============================================

void drawFactoryReset() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(20, "⚠️ FACTORY RESET");
    CenteredStr(35, "Delete all data?");
    CenteredStr(50, "SEL: Yes  BACK: No");
    u8g2.sendBuffer();
}

// =============================================
// SCAN WIFI NETWORKS
// =============================================

void scanWiFiNetworks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    
    if(n > 0) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        CenteredStr(10, "Networks Found");
        u8g2.drawLine(0, 12, 128, 12);
        
        int displayCount = min(n, 5);
        for(int i = 0; i < displayCount; i++) {
            char buf[20];
            String ssid = WiFi.SSID(i);
            if(ssid.length() == 0) ssid = "<Hidden>";
            sprintf(buf, "%d. %s", i + 1, ssid.substring(0, 15).c_str());
            u8g2.setCursor(5, 22 + i * 8);
            u8g2.print(buf);
            
            u8g2.setCursor(110, 22 + i * 8);
            u8g2.print(WiFi.RSSI(i));
        }
        u8g2.sendBuffer();
        delay(2000);
    } else {
        u8g2.clearBuffer();
        CenteredStr(30, "No networks found");
        u8g2.sendBuffer();
        delay(1000);
    }
    WiFi.scanDelete();
}

// =============================================
// HANDLE SET WIFI CREDENTIALS
// =============================================

void handleSetWiFiCredentials() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(20, "📡 WiFi Setup");
    CenteredStr(35, "Use Serial Monitor");
    CenteredStr(48, "Format: SSID,Password");
    CenteredStr(60, "Example: MyWiFi,1234");
    u8g2.sendBuffer();
    
    Serial.println("\n========================================");
    Serial.println("📡 WiFi Credentials Setup");
    Serial.println("========================================");
    Serial.println("Enter SSID and password:");
    Serial.println("Format: SSID,Password");
    Serial.println("Example: MyWiFi,MyPassword123");
    Serial.println("Type 'CANCEL' to abort");
    Serial.println("========================================\n");
    
    unsigned long start = millis();
    String input = "";
    
    while (millis() - start < 30000) {
        if (Serial.available()) {
            input = Serial.readStringUntil('\n');
            input.trim();
            break;
        }
        delay(100);
        
        if ((millis() - start) % 5000 < 100) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tr);
            CenteredStr(20, "📡 WiFi Setup");
            CenteredStr(35, "Waiting for input...");
            char timeStr[20];
            sprintf(timeStr, "%d/30 seconds", (millis() - start) / 1000);
            CenteredStr(48, timeStr);
            CenteredStr(60, "Type in Serial Monitor");
            u8g2.sendBuffer();
        }
    }
    
    if (input.length() == 0) {
        showPopupMessage("⏰ Timeout!", "No input received", "");
        delay(1000);
        drawWifiSettings();
        return;
    }
    
    if (input == "CANCEL" || input == "cancel") {
        showPopupMessage("❌ Cancelled", "", "");
        delay(1000);
        drawWifiSettings();
        return;
    }
    
    int comma = input.indexOf(',');
    if (comma > 0) {
        String ssid = input.substring(0, comma);
        String password = input.substring(comma + 1);
        ssid.trim();
        password.trim();
        
        if (ssid.length() > 0 && password.length() > 0) {
            saveWiFiCredentials(ssid.c_str(), password.c_str());
            Serial.println("✅ WiFi credentials saved!");
            Serial.printf("  SSID: %s\n", ssid.c_str());
            showPopupMessage("✅ Credentials Saved!", ssid.c_str(), "");
            delay(1000);
            
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tr);
            CenteredStr(25, "Connecting to WiFi...");
            CenteredStr(40, ssid.c_str());
            u8g2.sendBuffer();
            
            connectToWiFi();
            drawWifiSettings();
        } else {
            showPopupMessage("❌ Invalid Input!", "SSID or Password empty", "");
            delay(1000);
            drawWifiSettings();
        }
    } else {
        showPopupMessage("❌ Invalid Format!", "Use: SSID,Password", "");
        delay(1000);
        drawWifiSettings();
    }
}

// =============================================
// HANDLE MAIN MENU
// =============================================

void handleMainMenu() {
    if(isButtonPressed(BTN_UP)) {
        settingsSelection = (settingsSelection - 1 + settingsCount) % settingsCount;
        drawSettingsMenu();
    }
    if(isButtonPressed(BTN_DOWN)) {
        settingsSelection = (settingsSelection + 1) % settingsCount;
        drawSettingsMenu();
    }
    if(isButtonPressed(BTN_SELECT)) {
        switch(settingsSelection) {
            case 0:
                settingsState = SETTINGS_BRIGHTNESS;
                drawBrightnessDisplay();
                break;
            case 1:
                settingsState = SETTINGS_WIFI;
                settingsSelection = 0;
                drawWifiSettings();
                break;
            case 2:
                settingsState = SETTINGS_TIME;
                timeState = TIME_MAIN;
                timeSelection = 0;
                drawTimeMainMenu();
                break;
            case 3:
                deepSleepEnabled = !deepSleepEnabled;
                setDeepSleepEnabled(deepSleepEnabled);
                showPopupMessage(deepSleepEnabled ? "💤 Sleep ON" : "💤 Sleep OFF", 
                                deepSleepEnabled ? "30s timeout" : "Disabled", "");
                delay(1000);
                drawSettingsMenu();
                break;
            case 4:
                settingsState = SETTINGS_SYSTEM;
                drawSystemInfo();
                break;
            case 5:
                settingsState = SETTINGS_STORAGE;
                drawStorageInfo();
                break;
            case 6:
                settingsState = SETTINGS_FACTORY_RESET;
                drawFactoryReset();
                break;
            case 7:
                exitModule();
                return;
        }
    }
}

// =============================================
// HANDLE BRIGHTNESS
// =============================================

void handleBrightness() {
    if(isButtonPressed(BTN_UP)) {
        oledBrightness = min(255, oledBrightness + 10);
        u8g2.setContrast(oledBrightness);
        EEPROM.write(0, oledBrightness);
        EEPROM.commit();
        drawBrightnessDisplay();
    }
    if(isButtonPressed(BTN_DOWN)) {
        oledBrightness = max(10, oledBrightness - 10);
        u8g2.setContrast(oledBrightness);
        EEPROM.write(0, oledBrightness);
        EEPROM.commit();
        drawBrightnessDisplay();
    }
    if(isButtonPressed(BTN_BACK) || isButtonPressed(BTN_SELECT)) {
        settingsState = SETTINGS_MAIN;
        drawSettingsMenu();
    }
}

// =============================================
// HANDLE WIFI MENU
// =============================================

void handleWifiMenu() {
    if(isButtonPressed(BTN_UP)) {
        settingsSelection = (settingsSelection - 1 + wifiCount) % wifiCount;
        drawWifiSettings();
    }
    if(isButtonPressed(BTN_DOWN)) {
        settingsSelection = (settingsSelection + 1) % wifiCount;
        drawWifiSettings();
    }
    if(isButtonPressed(BTN_SELECT)) {
        switch(settingsSelection) {
            case 0:
                WiFi.mode(WIFI_STA);
                showPopupMessage("WiFi Enabled", "", "");
                delay(500);
                drawWifiSettings();
                break;
            case 1:
                WiFi.mode(WIFI_OFF);
                showPopupMessage("WiFi Disabled", "", "");
                delay(500);
                drawWifiSettings();
                break;
            case 2:
                scanWiFiNetworks();
                drawWifiSettings();
                break;
            case 3:
                handleSetWiFiCredentials();
                break;
            case 4:
                clearWiFiCredentials();
                showPopupMessage("🗑️ Credentials Cleared", "", "");
                delay(500);
                drawWifiSettings();
                break;
            case 5:
                settingsState = SETTINGS_MAIN;
                drawSettingsMenu();
                break;
        }
    }
    if(isButtonPressed(BTN_BACK)) {
        settingsState = SETTINGS_MAIN;
        drawSettingsMenu();
    }
}

// =============================================
// HANDLE TIME MAIN
// =============================================

void handleTimeMain() {
    if(isButtonPressed(BTN_UP)) {
        timeSelection = (timeSelection - 1 + 5) % 5;
        drawTimeMainMenu();
    }
    if(isButtonPressed(BTN_DOWN)) {
        timeSelection = (timeSelection + 1) % 5;
        drawTimeMainMenu();
    }
    if(isButtonPressed(BTN_SELECT)) {
        switch(timeSelection) {
            case 0:
                timeState = TIME_MANUAL;
                timeEditIndex = 0;
                editHour = watchHours;
                editMinute = watchMinutes;
                editSecond = watchSeconds;
                editDay = watchDay;
                editMonth = watchMonth;
                editYear = watchYear;
                drawTimeManual();
                break;
            case 1:
                timeState = TIME_NTP;
                ntpSyncing = false;
                ntpSyncSuccess = false;
                drawTimeNTP();
                break;
            case 2:
                timeState = TIME_TIMEZONE;
                drawTimeTimezone();
                break;
            case 3:
                timeState = TIME_FORMAT;
                timeSelection = use12HourFormat ? 0 : 1;
                drawTimeFormat();
                break;
            case 4:
                settingsState = SETTINGS_MAIN;
                drawSettingsMenu();
                break;
        }
    }
    if(isButtonPressed(BTN_BACK)) {
        settingsState = SETTINGS_MAIN;
        drawSettingsMenu();
    }
}

// =============================================
// HANDLE TIME MANUAL
// =============================================

void handleTimeManual() {
    static unsigned long lastButtonTime = 0;
    unsigned long now = millis();
    
    if (now - lastButtonTime < 200) {
        delay(10);
        return;
    }
    
    if (isButtonPressed(BTN_UP)) {
        lastButtonTime = now;
        int field = timeEditIndex % 3;
        switch(field) {
            case 0: editHour = (editHour + 1) % 24; break;
            case 1: editMinute = (editMinute + 1) % 60; break;
            case 2: editSecond = (editSecond + 1) % 60; break;
        }
        drawTimeManual();
    }
    
    if (isButtonPressed(BTN_DOWN)) {
        lastButtonTime = now;
        int field = timeEditIndex % 3;
        switch(field) {
            case 0: editHour = (editHour - 1 + 24) % 24; break;
            case 1: editMinute = (editMinute - 1 + 60) % 60; break;
            case 2: editSecond = (editSecond - 1 + 60) % 60; break;
        }
        drawTimeManual();
    }
    
    if (isButtonPressed(BTN_SELECT)) {
        lastButtonTime = now;
        timeEditIndex = (timeEditIndex + 1) % 6;
        drawTimeManual();
    }
    
    if (isButtonPressed(BTN_BACK)) {
        lastButtonTime = now;
        saveTimeToRTC();
        showPopupMessage("✅ Time Saved!", currentTimeStr.c_str(), "");
        delay(1000);
        timeState = TIME_MAIN;
        drawTimeMainMenu();
    }
}

// =============================================
// HANDLE TIME NTP
// =============================================

void handleTimeNTP() {
    if (isButtonPressed(BTN_UP)) {
        selectedNtpServer = (selectedNtpServer - 1 + ntpServerCount) % ntpServerCount;
        drawTimeNTP();
    }
    if (isButtonPressed(BTN_DOWN)) {
        selectedNtpServer = (selectedNtpServer + 1) % ntpServerCount;
        drawTimeNTP();
    }
    if (isButtonPressed(BTN_SELECT) && !ntpSyncing) {
        syncTimeWithNTP(ntpServers[selectedNtpServer]);
        drawTimeNTP();
    }
    if (isButtonPressed(BTN_BACK)) {
        timeState = TIME_MAIN;
        drawTimeMainMenu();
    }
}

// =============================================
// HANDLE TIME TIMEZONE
// =============================================

void handleTimeTimezone() {
    if (isButtonPressed(BTN_UP)) {
        timezoneIndex = (timezoneIndex - 1 + timezoneCount) % timezoneCount;
        timezoneOffset = getTimezoneOffsetFromIndex(timezoneIndex);
        drawTimeTimezone();
    }
    if (isButtonPressed(BTN_DOWN)) {
        timezoneIndex = (timezoneIndex + 1) % timezoneCount;
        timezoneOffset = getTimezoneOffsetFromIndex(timezoneIndex);
        drawTimeTimezone();
    }
    if (isButtonPressed(BTN_SELECT)) {
        saveTimezoneToEEPROM();
        applyTimezone();
        showPopupMessage("✅ Timezone Saved!", timezoneNames[timezoneIndex], "");
        delay(1000);
        timeState = TIME_MAIN;
        drawTimeMainMenu();
    }
    if (isButtonPressed(BTN_BACK)) {
        timeState = TIME_MAIN;
        drawTimeMainMenu();
    }
}

// =============================================
// HANDLE TIME FORMAT
// =============================================

void handleTimeFormat() {
    if (isButtonPressed(BTN_UP)) {
        timeSelection = (timeSelection - 1 + 3) % 3;
        drawTimeFormat();
    }
    if (isButtonPressed(BTN_DOWN)) {
        timeSelection = (timeSelection + 1) % 3;
        drawTimeFormat();
    }
    if (isButtonPressed(BTN_SELECT)) {
        if (timeSelection == 0) {
            use12HourFormat = true;
            saveTimeFormatToEEPROM();
            showPopupMessage("✅ 12-Hour Format", "", "");
            delay(800);
        } else if (timeSelection == 1) {
            use12HourFormat = false;
            saveTimeFormatToEEPROM();
            showPopupMessage("✅ 24-Hour Format", "", "");
            delay(800);
        } else {
            timeState = TIME_MAIN;
            drawTimeMainMenu();
            return;
        }
        extern void updateWatchTime();
        updateWatchTime();
        drawTimeFormat();
    }
    if (isButtonPressed(BTN_BACK)) {
        timeState = TIME_MAIN;
        drawTimeMainMenu();
    }
}

// =============================================
// HANDLE SYSTEM INFO
// =============================================

void handleSystemInfo() {
    if(isButtonPressed(BTN_BACK) || isButtonPressed(BTN_SELECT)) {
        settingsState = SETTINGS_MAIN;
        drawSettingsMenu();
    }
    drawSystemInfo();
}

// =============================================
// HANDLE STORAGE INFO
// =============================================

void handleStorageInfo() {
    if(isButtonPressed(BTN_BACK) || isButtonPressed(BTN_SELECT)) {
        settingsState = SETTINGS_MAIN;
        drawSettingsMenu();
    }
    drawStorageInfo();
}

// =============================================
// HANDLE FACTORY RESET
// =============================================

void handleFactoryReset() {
    if(isButtonPressed(BTN_SELECT)) {
        EEPROM.write(0, 128);
        EEPROM.write(50, 20);
        EEPROM.write(51, 0);
        EEPROM.write(52, 1);
        EEPROM.commit();
        
        credCount = 0;
        saveCredentials();
        clearWiFiCredentials();
        
        u8g2.clearBuffer();
        CenteredStr(25, "Factory Reset Done!");
        CenteredStr(40, "Brightness reset");
        CenteredStr(55, "Creds & WiFi cleared");
        u8g2.sendBuffer();
        delay(1500);
        
        settingsState = SETTINGS_MAIN;
        drawSettingsMenu();
    }
    if(isButtonPressed(BTN_BACK)) {
        settingsState = SETTINGS_MAIN;
        drawSettingsMenu();
    }
}

// =============================================
// SETTINGS SETUP
// =============================================

void settingsSetup() {
    loadCredentials();
    loadWiFiCredentials();
    loadTimezoneFromEEPROM();
    loadTimeFormatFromEEPROM();
    isDeepSleepEnabled();  // Load sleep setting
    
    settingsState = SETTINGS_MAIN;
    settingsSelection = 0;
    clearPopup();
    drawSettingsMenu();
}

// =============================================
// SETTINGS LOOP
// =============================================

void settingsLoop() {
    if (drawPopupIfActive()) {
        delay(50);
        return;
    }
    
    if(isButtonPressed(BTN_BACK)) {
        if(settingsState == SETTINGS_MAIN) {
            exitModule();
            return;
        } else {
            settingsState = SETTINGS_MAIN;
            settingsSelection = 0;
            drawSettingsMenu();
            return;
        }
    }
    
    switch(settingsState) {
        case SETTINGS_MAIN:
            handleMainMenu();
            break;
            
        case SETTINGS_BRIGHTNESS:
            handleBrightness();
            break;
            
        case SETTINGS_WIFI:
            handleWifiMenu();
            break;
            
        case SETTINGS_TIME:
            handleTimeMain();
            break;
            
        case SETTINGS_TIME_MANUAL:
            handleTimeManual();
            break;
            
        case SETTINGS_TIME_NTP:
            handleTimeNTP();
            break;
            
        case SETTINGS_TIMEZONE:
            handleTimeTimezone();
            break;
            
        case SETTINGS_SYSTEM:
            handleSystemInfo();
            break;
            
        case SETTINGS_STORAGE:
            handleStorageInfo();
            break;
            
        case SETTINGS_FACTORY_RESET:
            handleFactoryReset();
            break;
    }
    
    delay(20);
}

// =============================================
// SETTINGS CLEANUP
// =============================================

void settingsCleanup() {
    Serial.println("🧹 Settings cleanup...");
    saveTimezoneToEEPROM();
    saveTimeFormatToEEPROM();
    saveTimeToRTC();
    
    settingsState = SETTINGS_MAIN;
    settingsSelection = 0;
    timeState = TIME_MAIN;
    timeSelection = 0;
    timeEditIndex = 0;
    clearPopup();
    
    Serial.println("✅ Settings cleanup complete");
}