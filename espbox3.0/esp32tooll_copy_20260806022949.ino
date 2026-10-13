// ESP32_Multi_Tool.ino
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ArduinoJson.h>
#include <time.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_timer.h>

// Include all headers
#include "pins.h"
#include "config.h"
#include "icons.h"
#include "utils.h"
#include "menu.h"
#include "chat.h"
#include "rogue_ap.h"
#include "phishing_pages.h"
#include "credentials.h"
#include "wifi_scanner.h"
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
#include "ereader.h"  // ✅ E-READER HEADER

// =================================================
// WIFI CREDENTIALS FOR TIME SYNC
// =================================================
const char* wifi_ssid = "FPV_CAM";
const char* wifi_password = "12345678";

// =================================================
// GLOBAL OBJECTS
// =================================================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(WS_PORT);
DNSServer dnsServer;

IRsend irsend(IR_SEND_PIN);
IRrecv irrecv(IR_RECV_PIN);
decode_results irResults;

uint8_t oledBrightness = 128;
bool wifiAPActive = false;
String currentAPSSID = "";

// Battery variables
int batteryLevel = 85;
int batteryVoltage = 0;

// =================================================
// CASIO WATCH TIME VARIABLES
// =================================================
unsigned long lastTimeUpdate = 0;
String currentTimeStr = "00:00:00";
String currentHours = "00";
String currentMinutes = "00";
String currentSeconds = "00";
String currentWeekdayStr = "SUN";  // ✅ ADDED - was missing
bool menuLoaded = false;
bool showWatchFace = true;

// Watch state (using int for compatibility with other modules)
int day = 1, month = 1, year = 2024;  // ✅ ADDED - was missing
int watchDay = 1, watchMonth = 1, watchYear = 2024;
int watchWeekDay = 3;

// RTC Memory for time persistence
RTC_DATA_ATTR uint64_t rtc_unixTime = 0;
RTC_DATA_ATTR uint64_t rtc_sleepEnterTime = 0;
RTC_DATA_ATTR bool rtc_timeSet = false;
RTC_DATA_ATTR bool rtc_timeSynced = false;
RTC_DATA_ATTR unsigned long rtc_bootCount = 0;

// Watch state
int watchHours = 12, watchMinutes = 0, watchSeconds = 0;

// NTP Configuration
const char* ntpServers[] = {
  "216.239.35.0",
  "216.239.35.4",
  "time.google.com",
  "pool.ntp.org",
  "0.pool.ntp.org",
  "1.pool.ntp.org"
};
const int ntpServerCount = 6;
const long gmtOffset_sec = 19800;  // IST (UTC+5:30)
const int daylightOffset_sec = 0;

// =================================================
// WATCH FUNCTIONS
// =================================================

int getDaysInMonth(int month, int year) {
  if (month == 2) {
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 29;
    return 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

void convertUnixToWatchTime(uint64_t unixTime) {
  uint64_t localTime = unixTime + 19800;  // IST
  
  watchSeconds = localTime % 60;
  uint64_t totalMinutes = localTime / 60;
  watchMinutes = totalMinutes % 60;
  uint64_t totalHours = totalMinutes / 60;
  watchHours = totalHours % 24;
  
  uint64_t days = totalHours / 24;
  watchYear = 1970;
  
  while (days >= 365) {
    int daysInYear = ((watchYear % 4 == 0 && watchYear % 100 != 0) || (watchYear % 400 == 0)) ? 366 : 365;
    if (days >= (uint64_t)daysInYear) {
      days -= daysInYear;
      watchYear++;
    } else {
      break;
    }
  }
  
  watchMonth = 1;
  while (true) {
    int daysInMonth = getDaysInMonth(watchMonth, watchYear);
    if (days >= (uint64_t)daysInMonth) {
      days -= daysInMonth;
      watchMonth++;
    } else {
      break;
    }
  }
  
  watchDay = days + 1;
  uint64_t totalDaysSince1970 = (totalHours / 24);
  watchWeekDay = (4 + totalDaysSince1970) % 7;
}

void updateWatchTime() {
  if (rtc_timeSet && rtc_unixTime > 0) {
    convertUnixToWatchTime(rtc_unixTime);
  } else {
    watchHours = 12;
    watchMinutes = 0;
    watchSeconds = 0;
    watchDay = 1;
    watchMonth = 1;
    watchYear = 2024;
    watchWeekDay = 3;
  }
  
  char timeStr[9];
  snprintf(timeStr, 9, "%02d:%02d:%02d", watchHours, watchMinutes, watchSeconds);
  currentTimeStr = String(timeStr);
  
  char hStr[3], mStr[3], sStr[3];
  snprintf(hStr, 3, "%02d", watchHours);
  snprintf(mStr, 3, "%02d", watchMinutes);
  snprintf(sStr, 3, "%02d", watchSeconds);
  currentHours = String(hStr);
  currentMinutes = String(mStr);
  currentSeconds = String(sStr);
  
  // Update day/month for display
  day = watchDay;
  month = watchMonth;
  year = watchYear;
  
  // Update weekday string
  String days[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  currentWeekdayStr = days[watchWeekDay];
}

String getWatchWeekday() {
  String days[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  return days[watchWeekDay];
}

// =================================================
// CASIO WATCH FACE - DEFAULT SCREEN
// =================================================
// =================================================
// CASIO WATCH FACE - ADJUSTED LAYOUT
// =================================================
void drawWatchFace() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  
  // ===== CASIO STYLE TIME - Hours : Minutes =====
  // Hours - Large (shifted right toward colon)
  u8g2.setFont(u8g2_font_logisoso32_tf);
  u8g2.setCursor(20, 44);  // Was 12, now 20 (shifted right)
  u8g2.print(currentHours.c_str());
  
  // Colon - Large
  u8g2.setCursor(62, 44);
  u8g2.print(":");
  
  // Minutes - Large
  u8g2.setCursor(76, 44);
  u8g2.print(currentMinutes.c_str());
  
  // ===== SECONDS - Centered below colon =====
  u8g2.setFont(u8g2_font_6x13_tr);
  int secWidth = u8g2.getStrWidth(currentSeconds.c_str());
  u8g2.setCursor((128 - secWidth) / 2, 54);  // Centered below colon
  u8g2.print(currentSeconds.c_str());
  
  // ===== DATE - Centered below seconds =====
  u8g2.setFont(u8g2_font_5x7_tf);
  char dateStr[20];
  sprintf(dateStr, "%s %02d/%02d", currentWeekdayStr.c_str(), day, month);
  int dateWidth = u8g2.getStrWidth(dateStr);
  u8g2.setCursor((128 - dateWidth) / 2, 62);  // Centered below seconds
  u8g2.print(dateStr);
  
  // ===== SYNC DOT INDICATOR (top right) =====
  if (rtc_timeSynced) {
    u8g2.drawBox(120, 2, 4, 4);
  }
  
  u8g2.sendBuffer();
}

// =================================================
// NTP TIME SYNC
// =================================================
bool tryGetNTPTimeWithFallback() {
  struct tm timeinfo;
  bool success = false;
  
  for (int serverIdx = 0; serverIdx < ntpServerCount && !success; serverIdx++) {
    Serial.printf("Trying NTP server: %s\n", ntpServers[serverIdx]);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServers[serverIdx]);
    
    for (int attempt = 0; attempt < 3 && !success; attempt++) {
      if (getLocalTime(&timeinfo, 8000)) {
        time_t testTime = mktime(&timeinfo);
        if (testTime > 1577836800) {
          success = true;
          break;
        }
      }
      if (!success) delay(1000);
    }
  }
  return success;
}

bool syncTimeWithWiFiBackground() {
  Serial.println("Background time sync starting...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  
  int attempts = 0;
  bool wifiConnected = false;
  
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(200);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("✅ WiFi connected for time sync");
    
    bool timeReceived = tryGetNTPTimeWithFallback();
    
    if (timeReceived) {
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      
      rtc_unixTime = (uint64_t)mktime(&timeinfo);
      rtc_timeSet = true;
      rtc_timeSynced = true;
      updateWatchTime();
      
      Serial.printf("✅ Time sync successful: %02d:%02d:%02d\n", 
                    watchHours, watchMinutes, watchSeconds);
    } else {
      Serial.println("❌ NTP failed");
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    Serial.println("❌ WiFi connection failed");
  }
  
  return wifiConnected;
}

// =================================================
// BATTERY FUNCTIONS
// =================================================

int readBatteryMillivolts() {
  int adcValue = analogRead(BATTERY_PIN);
  float voltage = (adcValue * 3.3) / 4095.0;
  voltage = voltage * 2.0;
  return (int)(voltage * 1000);
}

int getBatteryPercentage() {
  int mV = readBatteryMillivolts();
  if (mV >= 4200) return 100;
  if (mV <= 3000) return 0;
  int percentage = (int)((mV - 3000) / (4200.0 - 3000.0) * 100);
  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;
  return percentage;
}

// =================================================
// MODULE MANAGER
// =================================================

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

// Global module manager
ModuleManager moduleManager;

// =================================================
// SETUP
// =================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP32 MULTI-TOOL v3.0 ===\n");
  Serial.println("Developed by @arjun_tanwar3337\n");
  
  // Enable PSRAM
  if (psramFound()) {
    Serial.println("✅ PSRAM Found!");
    Serial.printf("📊 PSRAM Size: %d KB\n", ESP.getPsramSize() / 1024);
    Serial.printf("📊 Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
  } else {
    Serial.println("⚠️ PSRAM Not Found");
  }
  
  // Battery ADC
  analogReadResolution(12);
  
  EEPROM.begin(EEPROM_SIZE);
  
  // I2C for OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  
  // Initialize OLED
  u8g2.begin();
  oledBrightness = EEPROM.read(0);
  if(oledBrightness < 10 || oledBrightness > 255) oledBrightness = 128;
  u8g2.setContrast(oledBrightness);
  
  // ===== LOAD RTC TIME =====
  if (rtc_timeSet && rtc_unixTime > 0) {
    if (rtc_unixTime < 1577836800) {
      rtc_unixTime = 1704067200;
      rtc_timeSet = true;
      rtc_timeSynced = false;
    }
  } else {
    rtc_unixTime = 1704067200;
    rtc_timeSet = true;
    rtc_timeSynced = false;
  }
  
  updateWatchTime();
  
  // ===== INIT SPIFFS =====
  Serial.println("Mounting SPIFFS...");
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS Mount Failed - Formatting...");
    if (SPIFFS.format()) {
      Serial.println("✅ SPIFFS Formatted");
      SPIFFS.begin(true);
    }
  } else {
    Serial.println("✅ SPIFFS Mounted");
  }
  
  // ===== SHOW WATCH FACE IMMEDIATELY =====
  drawWatchFace();
  
  // Initialize Pins
  for(int i=0; i<4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    Serial.printf("Pin %d: %d\n", buttonPins[i], digitalRead(buttonPins[i]));
  }
  delay(200);
  
  // Get battery level
  batteryLevel = getBatteryPercentage();
  delay(200);
  
  // Load Credentials
  loadCredentials();
  delay(200);
  
  // Configure WiFi
  disableWiFi();
  delay(100);
  
  // Load Modules (includes E-Reader)
  initMenu();
  delay(200);
  
  // Print storage info
  printStorageInfo();
  
  // Set flags
  menuLoaded = true;
  showWatchFace = true;
  
  // Background time sync on first boot
  if (!rtc_timeSynced) {
    Serial.println("Starting background time sync...");
    syncTimeWithWiFiBackground();
    drawWatchFace();
  }
  
  Serial.println("✅ ALL MODULES READY!");
  Serial.printf("📊 Credentials: %d\n", credCount);
  Serial.printf("📊 Modules: %d\n", itemCount);
  Serial.printf("📊 Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
  Serial.println("\n💡 Press any button to enter menu");
  Serial.println("💡 E-Reader is in the main menu");
  Serial.println("💡 BACK = Return to Watch Face");
}

// =================================================
// LOOP
// =================================================
void loop() {
  webServer.handleClient();
  
  // Update battery every 5 seconds
  static unsigned long lastBattery = 0;
  if (millis() - lastBattery > 5000) {
    lastBattery = millis();
    batteryLevel = getBatteryPercentage();
  }
  
  // Update time every second
  if (millis() - lastTimeUpdate > 1000) {
    lastTimeUpdate = millis();
    rtc_unixTime++;
    updateWatchTime();
    
    if (showWatchFace && currentScreen == 0) {
      drawWatchFace();
    }
  }
  
  // ===== WATCH FACE - Wait for button press =====
  if (showWatchFace && menuLoaded) {
    bool anyButtonPressed = false;
    for (int i = 0; i < 4; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        anyButtonPressed = true;
        break;
      }
    }
    
    if (anyButtonPressed) {
      delay(100);
      // Wait for all buttons to be released
      bool allReleased = false;
      while (!allReleased) {
        allReleased = true;
        for (int i = 0; i < 4; i++) {
          if (digitalRead(buttonPins[i]) == LOW) {
            allReleased = false;
            break;
          }
        }
        if (!allReleased) delay(50);
      }
      
      showWatchFace = false;
      currentScreen = 0;
      drawMenu();
      Serial.println("📱 Menu loaded");
    }
    delay(10);
    return;
  }
  
  // ===== MAIN MENU =====
  if(currentScreen == 0) {
    if(isButtonPressed(BTN_UP)) { 
      selectedItem = (selectedItem - 1 + itemCount) % itemCount; 
      drawMenu(); 
    } else if(isButtonPressed(BTN_DOWN)) { 
      selectedItem = (selectedItem + 1) % itemCount; 
      drawMenu(); 
    } else if(isButtonPressed(BTN_SELECT)) {
      Serial.printf("Entering module: %d\n", selectedItem);
      moduleManager.enterModule(selectedItem);
    } else if(isButtonPressed(BTN_BACK)) {
      showWatchFace = true;
      drawWatchFace();
    }
  } else {
    if(menuItems[selectedItem].loopFunc != nullptr) {
      menuItems[selectedItem].loopFunc();
    }
  }
  delay(10);
}