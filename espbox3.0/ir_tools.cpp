#include "ir_tools.h"
#include "config.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <U8g2lib.h>
#include <WiFi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern IRsend irsend;
extern IRrecv irrecv;
extern decode_results irResults;
extern WebServer webServer;

// =================================================
// GLOBAL VARIABLES
// =================================================
IRCode irCodes[MAX_ACTIVE_CODES];
int irCodeCount = 0;

enum IRState { 
    IR_IDLE, 
    IR_LEARN, 
    IR_SEND, 
    IR_RANDOM, 
    IR_BLAST,
    IR_WEB_UI,
    IR_QUICK_CTRL
} irState = IR_IDLE;

int irMenuIdx = 0;
int irSendIdx = 0;
int irBlastCnt = 0;
unsigned long irLearnStart = 0;
bool irInitialized = false;
bool wifiEnabled = false;

// =================================================
// QUICK REMOTE - 4 BUTTONS
// =================================================
int quickSelection = 0;
int quickScrollTop = 0;
int quickDevice = 0;
const int QUICK_VISIBLE_ITEMS = 4;

// =================================================
// FORWARD DECLARATIONS
// =================================================
void showIRMsg(const char* l1, const char* l2, const char* l3);
void loadIRCodes();
void saveIRCodes();
void addIRCode(uint32_t code, String name, String device, String function);
void sendIRCode(uint32_t code);
void drawIRMenu();
void clearIRCodes();
void startLearn();
void processLearn();
void startSend();
void processSend();
void startRandom();
void startBlast();
void processBlast();
void startWebUI();
void setupIRWebServer();
String getIRWebPage();
void startQuickRemote();
void handleQuickRemote();
void drawQuickRemote();
void loadDeviceCodes(String device);
void clearActiveCodes();

// =================================================
// CODE DEFINITIONS - STORED IN FLASH
// =================================================

// DD Free Dish Codes
const uint32_t DD_POWER = 0x00FF906F;
const uint32_t DD_MUTE = 0x00FFC03F;
const uint32_t DD_MENU = 0x00FF38C7;
const uint32_t DD_OK = 0x00FFA05F;
const uint32_t DD_UP = 0x00FF708F;
const uint32_t DD_DOWN = 0x00FF58A7;
const uint32_t DD_LEFT = 0x00FF50AF;
const uint32_t DD_RIGHT = 0x00FF7887;
const uint32_t DD_VOL_UP = 0x00FF40BF;
const uint32_t DD_VOL_DOWN = 0x00FF30CF;
const uint32_t DD_PAUSE = 0x00FF2AD5;
const uint32_t DD_SIGNAL = 0x00FF32CD;
const uint32_t DD_VIEW = 0x00FF3AC5;
const uint32_t DD_AUDIO = 0x00FF8A75;
const uint32_t DD_NUM_1 = 0x00FF0CF3;
const uint32_t DD_NUM_2 = 0x00FF8C73;
const uint32_t DD_NUM_3 = 0x00FF4CB3;
const uint32_t DD_NUM_4 = 0x00FFCC33;
const uint32_t DD_NUM_5 = 0x00FF2CD3;
const uint32_t DD_NUM_6 = 0x00FFAC53;
const uint32_t DD_NUM_7 = 0x00FF6C93;
const uint32_t DD_NUM_8 = 0x00FFEC13;
const uint32_t DD_NUM_9 = 0x00FF1CE3;
const uint32_t DD_NUM_0 = 0x00FF9C63;

// Samsung Codes
const uint32_t SAMSUNG_POWER = 0xE0E09966;
const uint32_t SAMSUNG_VOL_UP = 0xE0E0E01F;
const uint32_t SAMSUNG_VOL_DOWN = 0xE0E0D02F;
const uint32_t SAMSUNG_MUTE = 0xE0E0F00F;
const uint32_t SAMSUNG_CH_UP = 0xE0E048B7;
const uint32_t SAMSUNG_CH_DOWN = 0xE0E008F7;
const uint32_t SAMSUNG_UP = 0xE0E006F9;
const uint32_t SAMSUNG_DOWN = 0xE0E08679;
const uint32_t SAMSUNG_LEFT = 0xE0E0A659;
const uint32_t SAMSUNG_RIGHT = 0xE0E046B9;
const uint32_t SAMSUNG_OK = 0xE0E016E9;
const uint32_t SAMSUNG_MENU = 0xE0E058A7;
const uint32_t SAMSUNG_SOURCE = 0xE0E0807F;
const uint32_t SAMSUNG_EXIT = 0xE0E0B44B;
const uint32_t SAMSUNG_NUM_1 = 0xE0E020DF;
const uint32_t SAMSUNG_NUM_2 = 0xE0E0A05F;
const uint32_t SAMSUNG_NUM_3 = 0xE0E0609F;
const uint32_t SAMSUNG_NUM_4 = 0xE0E010EF;
const uint32_t SAMSUNG_NUM_5 = 0xE0E0906F;
const uint32_t SAMSUNG_NUM_6 = 0xE0E050AF;
const uint32_t SAMSUNG_NUM_7 = 0xE0E030CF;
const uint32_t SAMSUNG_NUM_8 = 0xE0E0B04F;
const uint32_t SAMSUNG_NUM_9 = 0xE0E0708F;
const uint32_t SAMSUNG_NUM_0 = 0xE0E08877;

// LG Codes
const uint32_t LG_POWER = 0x20DF10EF;
const uint32_t LG_VOL_UP = 0x20DF40BF;
const uint32_t LG_VOL_DOWN = 0x20DFC03F;
const uint32_t LG_MUTE = 0x20DF906F;
const uint32_t LG_CH_UP = 0x20DF00FF;
const uint32_t LG_CH_DOWN = 0x20DF807F;
const uint32_t LG_UP = 0x20DF02FD;
const uint32_t LG_DOWN = 0x20DF827D;
const uint32_t LG_LEFT = 0x20DFE01F;
const uint32_t LG_RIGHT = 0x20DF609F;
const uint32_t LG_OK = 0x20DF22DD;
const uint32_t LG_MENU = 0x20DFC23D;
const uint32_t LG_INPUT = 0x20DFD02F;
const uint32_t LG_EXIT = 0x20DFDA25;
const uint32_t LG_NUM_1 = 0x20DF8877;
const uint32_t LG_NUM_2 = 0x20DF48B7;
const uint32_t LG_NUM_3 = 0x20DFC837;
const uint32_t LG_NUM_4 = 0x20DF28D7;
const uint32_t LG_NUM_5 = 0x20DFA857;
const uint32_t LG_NUM_6 = 0x20DF6897;
const uint32_t LG_NUM_7 = 0x20DFE817;
const uint32_t LG_NUM_8 = 0x20DF18E7;
const uint32_t LG_NUM_9 = 0x20DF9867;
const uint32_t LG_NUM_0 = 0x20DF08F7;

// Sony Codes
const uint32_t SONY_POWER = 0xA90;
const uint32_t SONY_VOL_UP = 0x490;
const uint32_t SONY_VOL_DOWN = 0xC90;
const uint32_t SONY_MUTE = 0x290;
const uint32_t SONY_CH_UP = 0x090;
const uint32_t SONY_CH_DOWN = 0x890;
const uint32_t SONY_UP = 0x2F0;
const uint32_t SONY_DOWN = 0xAF0;
const uint32_t SONY_LEFT = 0x2D0;
const uint32_t SONY_RIGHT = 0xCD0;
const uint32_t SONY_OK = 0xA70;
const uint32_t SONY_MENU = 0x070;
const uint32_t SONY_INPUT = 0xA50;
const uint32_t SONY_NUM_1 = 0x010;
const uint32_t SONY_NUM_2 = 0x810;
const uint32_t SONY_NUM_3 = 0x410;
const uint32_t SONY_NUM_4 = 0xC10;
const uint32_t SONY_NUM_5 = 0x210;
const uint32_t SONY_NUM_6 = 0xA10;
const uint32_t SONY_NUM_7 = 0x610;
const uint32_t SONY_NUM_8 = 0xE10;
const uint32_t SONY_NUM_9 = 0x110;
const uint32_t SONY_NUM_0 = 0x910;

// TCL Codes
const uint32_t TCL_POWER = 0x0D5F2A;
const uint32_t TCL_VOL_UP = 0x0D0F2F;
const uint32_t TCL_VOL_DOWN = 0x0D1F2E;
const uint32_t TCL_MUTE = 0x0C0F3F;
const uint32_t TCL_CH_UP = 0x0D2F2D;
const uint32_t TCL_CH_DOWN = 0x0D3F2C;
const uint32_t TCL_UP = 0x0A6F59;
const uint32_t TCL_DOWN = 0x0A7F58;
const uint32_t TCL_LEFT = 0x0A9F56;
const uint32_t TCL_RIGHT = 0x0A8F57;
const uint32_t TCL_OK = 0x00BFF4;
const uint32_t TCL_MENU = 0x013FEC;
const uint32_t TCL_BACK = 0x0D8F27;
const uint32_t TCL_SOURCE = 0x05CFA3;

// Panasonic Codes
const uint32_t PANASONIC_POWER = 0x40040100BCBD;
const uint32_t PANASONIC_VOL_UP = 0x400401000405;
const uint32_t PANASONIC_VOL_DOWN = 0x400401008485;
const uint32_t PANASONIC_MUTE = 0x400401004C4D;
const uint32_t PANASONIC_CH_UP = 0x400401002C2D;
const uint32_t PANASONIC_CH_DOWN = 0x40040100ACAD;
const uint32_t PANASONIC_UP = 0x400401005253;
const uint32_t PANASONIC_DOWN = 0x40040100D2D3;
const uint32_t PANASONIC_LEFT = 0x400401007273;
const uint32_t PANASONIC_RIGHT = 0x40040100F2F3;
const uint32_t PANASONIC_OK = 0x400401009293;
const uint32_t PANASONIC_MENU = 0x400401004A4B;
const uint32_t PANASONIC_SOURCE = 0x40040100A0A1;

// VEON Codes
const uint32_t VEON_POWER = 0x00FEA857;
const uint32_t VEON_VOL_UP = 0x00FED827;
const uint32_t VEON_VOL_DOWN = 0x00FE58A7;
const uint32_t VEON_MUTE = 0x00FE6897;
const uint32_t VEON_CH_UP = 0x00FE9867;
const uint32_t VEON_CH_DOWN = 0x00FE18E7;
const uint32_t VEON_UP = 0x00FE30CF;
const uint32_t VEON_DOWN = 0x00FEB04F;
const uint32_t VEON_LEFT = 0x00FEF00F;
const uint32_t VEON_RIGHT = 0x00FE708F;
const uint32_t VEON_OK = 0x00FE08F7;
const uint32_t VEON_MENU = 0x00FE8877;
const uint32_t VEON_SOURCE = 0x00FE48B7;

// =================================================
// QUICK REMOTE DEVICE ARRAYS
// =================================================

const char* quickDevices[] = {
    "DD Free Dish",
    "Samsung",
    "LG",
    "Sony",
    "TCL",
    "Panasonic",
    "VEON"
};
const int QUICK_DEVICE_COUNT = 7;

const char* quickMainItems[] = {
    "PWR  Power",
    "SRC  Source",
    "MENU Menu",
    "EXIT Exit",
    "VOL+ Volume Up",
    "VOL- Volume Down",
    "CH+  Channel Up",
    "CH-  Channel Down",
    "MUTE Mute",
    "▲    Up",
    "▼    Down",
    "◄    Left",
    "►    Right",
    "OK   Enter",
    "0-9  Numbers"
};
const int quickMainCount = 15;

// =================================================
// SPIFFS STORAGE FUNCTIONS
// =================================================

void loadIRCodes() {
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS mount failed");
        return;
    }
    
    if (!SPIFFS.exists(IR_FILE)) {
        Serial.println("📄 IR file not found, creating defaults...");
        preloadDefaultCodes();
        return;
    }
    
    File file = SPIFFS.open(IR_FILE, "r");
    if (!file) {
        Serial.println("❌ Failed to open IR file");
        return;
    }
    
    DynamicJsonDocument doc(65536);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("❌ Failed to parse IR JSON");
        return;
    }
    
    int totalCount = doc["count"] | 0;
    irCodeCount = 0;
    
    for (int i = 0; i < totalCount; i++) {
        String dev = doc["codes"][i]["device"] | "";
        if (dev == "DD Free Dish" && irCodeCount < MAX_ACTIVE_CODES) {
            irCodes[irCodeCount].code = doc["codes"][i]["code"] | 0;
            const char* name = doc["codes"][i]["name"];
            const char* function = doc["codes"][i]["function"];
            irCodes[irCodeCount].name = name ? String(name) : "Code " + String(irCodeCount + 1);
            irCodes[irCodeCount].device = dev;
            irCodes[irCodeCount].function = function ? String(function) : "Custom";
            irCodes[irCodeCount].active = true;
            irCodeCount++;
        }
    }
    
    Serial.printf("✅ Loaded %d DD Free Dish codes from SPIFFS\n", irCodeCount);
}

void loadDeviceCodes(String device) {
    if (device == "DD Free Dish") {
        clearActiveCodes();
        return;
    }
    
    int keepCount = 0;
    for (int i = 0; i < irCodeCount; i++) {
        if (irCodes[i].device == "DD Free Dish") {
            irCodes[keepCount] = irCodes[i];
            keepCount++;
        }
    }
    irCodeCount = keepCount;
    
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS mount failed");
        return;
    }
    
    if (!SPIFFS.exists(IR_FILE)) {
        Serial.println("❌ IR file not found");
        return;
    }
    
    File file = SPIFFS.open(IR_FILE, "r");
    if (!file) {
        Serial.println("❌ Failed to open IR file");
        return;
    }
    
    DynamicJsonDocument doc(65536);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("❌ Failed to parse IR JSON");
        return;
    }
    
    int totalCount = doc["count"] | 0;
    
    for (int i = 0; i < totalCount; i++) {
        String dev = doc["codes"][i]["device"] | "";
        if (dev == device && irCodeCount < MAX_ACTIVE_CODES) {
            irCodes[irCodeCount].code = doc["codes"][i]["code"] | 0;
            const char* name = doc["codes"][i]["name"];
            const char* function = doc["codes"][i]["function"];
            irCodes[irCodeCount].name = name ? String(name) : "Code " + String(irCodeCount + 1);
            irCodes[irCodeCount].device = dev;
            irCodes[irCodeCount].function = function ? String(function) : "Custom";
            irCodes[irCodeCount].active = true;
            irCodeCount++;
        }
    }
    
    Serial.printf("✅ Loaded %d codes for %s\n", irCodeCount - keepCount, device.c_str());
}

void clearActiveCodes() {
    int keepCount = 0;
    for (int i = 0; i < irCodeCount; i++) {
        if (irCodes[i].device == "DD Free Dish") {
            irCodes[keepCount] = irCodes[i];
            keepCount++;
        }
    }
    irCodeCount = keepCount;
    Serial.println("🔄 Cleared active codes");
}

void preloadDefaultCodes() {
    Serial.println("📦 Preloading ALL default IR codes to SPIFFS...");
    
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS mount failed");
        return;
    }
    
    DynamicJsonDocument doc(65536);
    JsonArray codes = doc.createNestedArray("codes");
    int count = 0;
    
    // Lambda to add code
    auto addCode = [&](uint32_t code, const char* name, const char* device, const char* function) {
        JsonObject obj = codes.createNestedObject();
        obj["code"] = code;
        obj["name"] = name;
        obj["device"] = device;
        obj["function"] = function;
        obj["active"] = true;
        char hexStr[16];
        sprintf(hexStr, "0x%08X", code);
        obj["hex"] = hexStr;
        count++;
    };
    
    // DD Free Dish (25 codes)
    addCode(DD_POWER, "Power", "DD Free Dish", "Power");
    addCode(DD_MUTE, "Mute", "DD Free Dish", "Volume");
    addCode(DD_MENU, "Menu", "DD Free Dish", "System");
    addCode(DD_OK, "OK", "DD Free Dish", "Navigation");
    addCode(DD_UP, "Up", "DD Free Dish", "Navigation");
    addCode(DD_DOWN, "Down", "DD Free Dish", "Navigation");
    addCode(DD_LEFT, "Left", "DD Free Dish", "Navigation");
    addCode(DD_RIGHT, "Right", "DD Free Dish", "Navigation");
    addCode(DD_VOL_UP, "Vol+", "DD Free Dish", "Volume");
    addCode(DD_VOL_DOWN, "Vol-", "DD Free Dish", "Volume");
    addCode(DD_PAUSE, "Pause", "DD Free Dish", "Playback");
    addCode(DD_SIGNAL, "Signal", "DD Free Dish", "Info");
    addCode(DD_VIEW, "View", "DD Free Dish", "View");
    addCode(DD_AUDIO, "Audio", "DD Free Dish", "Audio");
    addCode(DD_NUM_1, "1", "DD Free Dish", "Numeric");
    addCode(DD_NUM_2, "2", "DD Free Dish", "Numeric");
    addCode(DD_NUM_3, "3", "DD Free Dish", "Numeric");
    addCode(DD_NUM_4, "4", "DD Free Dish", "Numeric");
    addCode(DD_NUM_5, "5", "DD Free Dish", "Numeric");
    addCode(DD_NUM_6, "6", "DD Free Dish", "Numeric");
    addCode(DD_NUM_7, "7", "DD Free Dish", "Numeric");
    addCode(DD_NUM_8, "8", "DD Free Dish", "Numeric");
    addCode(DD_NUM_9, "9", "DD Free Dish", "Numeric");
    addCode(DD_NUM_0, "0", "DD Free Dish", "Numeric");
    
    // Samsung (33 codes)
    addCode(SAMSUNG_POWER, "Power", "Samsung", "Power");
    addCode(SAMSUNG_VOL_UP, "Vol+", "Samsung", "Volume");
    addCode(SAMSUNG_VOL_DOWN, "Vol-", "Samsung", "Volume");
    addCode(SAMSUNG_MUTE, "Mute", "Samsung", "Volume");
    addCode(SAMSUNG_CH_UP, "Ch+", "Samsung", "Channel");
    addCode(SAMSUNG_CH_DOWN, "Ch-", "Samsung", "Channel");
    addCode(SAMSUNG_UP, "Up", "Samsung", "Navigation");
    addCode(SAMSUNG_DOWN, "Down", "Samsung", "Navigation");
    addCode(SAMSUNG_LEFT, "Left", "Samsung", "Navigation");
    addCode(SAMSUNG_RIGHT, "Right", "Samsung", "Navigation");
    addCode(SAMSUNG_OK, "OK", "Samsung", "Navigation");
    addCode(SAMSUNG_MENU, "Menu", "Samsung", "System");
    addCode(SAMSUNG_SOURCE, "Source", "Samsung", "System");
    addCode(SAMSUNG_EXIT, "Exit", "Samsung", "System");
    addCode(SAMSUNG_NUM_1, "1", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_2, "2", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_3, "3", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_4, "4", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_5, "5", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_6, "6", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_7, "7", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_8, "8", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_9, "9", "Samsung", "Numeric");
    addCode(SAMSUNG_NUM_0, "0", "Samsung", "Numeric");
    
    // LG (33 codes)
    addCode(LG_POWER, "Power", "LG", "Power");
    addCode(LG_VOL_UP, "Vol+", "LG", "Volume");
    addCode(LG_VOL_DOWN, "Vol-", "LG", "Volume");
    addCode(LG_MUTE, "Mute", "LG", "Volume");
    addCode(LG_CH_UP, "Ch+", "LG", "Channel");
    addCode(LG_CH_DOWN, "Ch-", "LG", "Channel");
    addCode(LG_UP, "Up", "LG", "Navigation");
    addCode(LG_DOWN, "Down", "LG", "Navigation");
    addCode(LG_LEFT, "Left", "LG", "Navigation");
    addCode(LG_RIGHT, "Right", "LG", "Navigation");
    addCode(LG_OK, "OK", "LG", "Navigation");
    addCode(LG_MENU, "Menu", "LG", "System");
    addCode(LG_INPUT, "Input", "LG", "System");
    addCode(LG_EXIT, "Exit", "LG", "System");
    addCode(LG_NUM_1, "1", "LG", "Numeric");
    addCode(LG_NUM_2, "2", "LG", "Numeric");
    addCode(LG_NUM_3, "3", "LG", "Numeric");
    addCode(LG_NUM_4, "4", "LG", "Numeric");
    addCode(LG_NUM_5, "5", "LG", "Numeric");
    addCode(LG_NUM_6, "6", "LG", "Numeric");
    addCode(LG_NUM_7, "7", "LG", "Numeric");
    addCode(LG_NUM_8, "8", "LG", "Numeric");
    addCode(LG_NUM_9, "9", "LG", "Numeric");
    addCode(LG_NUM_0, "0", "LG", "Numeric");
    
    // Sony (31 codes)
    addCode(SONY_POWER, "Power", "Sony", "Power");
    addCode(SONY_VOL_UP, "Vol+", "Sony", "Volume");
    addCode(SONY_VOL_DOWN, "Vol-", "Sony", "Volume");
    addCode(SONY_MUTE, "Mute", "Sony", "Volume");
    addCode(SONY_CH_UP, "Ch+", "Sony", "Channel");
    addCode(SONY_CH_DOWN, "Ch-", "Sony", "Channel");
    addCode(SONY_UP, "Up", "Sony", "Navigation");
    addCode(SONY_DOWN, "Down", "Sony", "Navigation");
    addCode(SONY_LEFT, "Left", "Sony", "Navigation");
    addCode(SONY_RIGHT, "Right", "Sony", "Navigation");
    addCode(SONY_OK, "OK", "Sony", "Navigation");
    addCode(SONY_MENU, "Menu", "Sony", "System");
    addCode(SONY_INPUT, "Input", "Sony", "System");
    addCode(SONY_NUM_1, "1", "Sony", "Numeric");
    addCode(SONY_NUM_2, "2", "Sony", "Numeric");
    addCode(SONY_NUM_3, "3", "Sony", "Numeric");
    addCode(SONY_NUM_4, "4", "Sony", "Numeric");
    addCode(SONY_NUM_5, "5", "Sony", "Numeric");
    addCode(SONY_NUM_6, "6", "Sony", "Numeric");
    addCode(SONY_NUM_7, "7", "Sony", "Numeric");
    addCode(SONY_NUM_8, "8", "Sony", "Numeric");
    addCode(SONY_NUM_9, "9", "Sony", "Numeric");
    addCode(SONY_NUM_0, "0", "Sony", "Numeric");
    
    // TCL (14 codes)
    addCode(TCL_POWER, "Power", "TCL", "Power");
    addCode(TCL_VOL_UP, "Vol+", "TCL", "Volume");
    addCode(TCL_VOL_DOWN, "Vol-", "TCL", "Volume");
    addCode(TCL_MUTE, "Mute", "TCL", "Volume");
    addCode(TCL_CH_UP, "Ch+", "TCL", "Channel");
    addCode(TCL_CH_DOWN, "Ch-", "TCL", "Channel");
    addCode(TCL_UP, "Up", "TCL", "Navigation");
    addCode(TCL_DOWN, "Down", "TCL", "Navigation");
    addCode(TCL_LEFT, "Left", "TCL", "Navigation");
    addCode(TCL_RIGHT, "Right", "TCL", "Navigation");
    addCode(TCL_OK, "OK", "TCL", "Navigation");
    addCode(TCL_MENU, "Menu", "TCL", "System");
    addCode(TCL_SOURCE, "Source", "TCL", "System");
    addCode(TCL_BACK, "Back", "TCL", "System");
    
    // Panasonic (13 codes)
    addCode(PANASONIC_POWER, "Power", "Panasonic", "Power");
    addCode(PANASONIC_VOL_UP, "Vol+", "Panasonic", "Volume");
    addCode(PANASONIC_VOL_DOWN, "Vol-", "Panasonic", "Volume");
    addCode(PANASONIC_MUTE, "Mute", "Panasonic", "Volume");
    addCode(PANASONIC_CH_UP, "Ch+", "Panasonic", "Channel");
    addCode(PANASONIC_CH_DOWN, "Ch-", "Panasonic", "Channel");
    addCode(PANASONIC_UP, "Up", "Panasonic", "Navigation");
    addCode(PANASONIC_DOWN, "Down", "Panasonic", "Navigation");
    addCode(PANASONIC_LEFT, "Left", "Panasonic", "Navigation");
    addCode(PANASONIC_RIGHT, "Right", "Panasonic", "Navigation");
    addCode(PANASONIC_OK, "OK", "Panasonic", "Navigation");
    addCode(PANASONIC_MENU, "Menu", "Panasonic", "System");
    addCode(PANASONIC_SOURCE, "Source", "Panasonic", "System");
    
    // VEON (13 codes)
    addCode(VEON_POWER, "Power", "VEON", "Power");
    addCode(VEON_VOL_UP, "Vol+", "VEON", "Volume");
    addCode(VEON_VOL_DOWN, "Vol-", "VEON", "Volume");
    addCode(VEON_MUTE, "Mute", "VEON", "Volume");
    addCode(VEON_CH_UP, "Ch+", "VEON", "Channel");
    addCode(VEON_CH_DOWN, "Ch-", "VEON", "Channel");
    addCode(VEON_UP, "Up", "VEON", "Navigation");
    addCode(VEON_DOWN, "Down", "VEON", "Navigation");
    addCode(VEON_LEFT, "Left", "VEON", "Navigation");
    addCode(VEON_RIGHT, "Right", "VEON", "Navigation");
    addCode(VEON_OK, "OK", "VEON", "Navigation");
    addCode(VEON_MENU, "Menu", "VEON", "System");
    addCode(VEON_SOURCE, "Source", "VEON", "System");
    
    doc["count"] = count;
    doc["version"] = "2.0";
    doc["date"] = __DATE__;
    
    File file = SPIFFS.open(IR_FILE, "w");
    if (file) {
        if (serializeJson(doc, file) > 0) {
            Serial.printf("✅ Saved ALL %d codes to SPIFFS\n", count);
        } else {
            Serial.println("❌ Failed to write IR JSON");
        }
        file.close();
    } else {
        Serial.println("❌ Failed to open IR file for writing");
    }
    
    File readFile = SPIFFS.open(IR_FILE, "r");
    if (readFile) {
        DynamicJsonDocument loadDoc(65536);
        deserializeJson(loadDoc, readFile);
        readFile.close();
        
        int totalCount = loadDoc["count"] | 0;
        irCodeCount = 0;
        
        for (int i = 0; i < totalCount; i++) {
            String dev = loadDoc["codes"][i]["device"] | "";
            if (dev == "DD Free Dish" && irCodeCount < MAX_ACTIVE_CODES) {
                irCodes[irCodeCount].code = loadDoc["codes"][i]["code"] | 0;
                const char* name = loadDoc["codes"][i]["name"];
                const char* function = loadDoc["codes"][i]["function"];
                irCodes[irCodeCount].name = name ? String(name) : "Code " + String(irCodeCount + 1);
                irCodes[irCodeCount].device = dev;
                irCodes[irCodeCount].function = function ? String(function) : "Custom";
                irCodes[irCodeCount].active = true;
                irCodeCount++;
            }
        }
        Serial.printf("✅ Loaded %d DD Free Dish codes into RAM\n", irCodeCount);
    }
    
    Serial.printf("📊 Total %d codes stored in SPIFFS\n", count);
}

void addIRCode(uint32_t code, String name, String device, String function) {
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS mount failed");
        return;
    }
    
    DynamicJsonDocument doc(65536);
    JsonArray codes;
    int totalCount = 0;
    
    if (SPIFFS.exists(IR_FILE)) {
        File file = SPIFFS.open(IR_FILE, "r");
        if (file) {
            deserializeJson(doc, file);
            totalCount = doc["count"] | 0;
            codes = doc["codes"].as<JsonArray>();
            file.close();
        }
    } else {
        codes = doc.createNestedArray("codes");
    }
    
    for (JsonObject obj : codes) {
        if (obj["code"] == code) {
            return;
        }
    }
    
    JsonObject obj = codes.createNestedObject();
    obj["code"] = code;
    obj["name"] = name;
    obj["device"] = device;
    obj["function"] = function;
    obj["active"] = true;
    
    char hexStr[16];
    sprintf(hexStr, "0x%08X", code);
    obj["hex"] = hexStr;
    
    doc["count"] = totalCount + 1;
    doc["version"] = "2.0";
    doc["date"] = __DATE__;
    
    File file = SPIFFS.open(IR_FILE, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.printf("✅ Added IR code to SPIFFS: %s\n", name.c_str());
    }
}

void saveIRCodes() {
    Serial.println("💾 IR codes saved to SPIFFS");
}

// =================================================
// SHOW IR MESSAGE
// =================================================

void showIRMsg(const char* l1, const char* l2, const char* l3) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_ir);
    CenteredStr(25, l1);
    if (strlen(l2) > 0) CenteredStr(40, l2);
    if (strlen(l3) > 0) CenteredStr(55, l3);
    u8g2.sendBuffer();
}

void sendIRCode(uint32_t code) {
    if (!irInitialized) {
        irsend.begin();
        irInitialized = true;
    }
    irsend.sendNEC(code, 32);
    delay(50);
}

// =================================================
// DRAW QUICK REMOTE - 4 BUTTONS
// =================================================

void drawQuickRemote() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_ir);
    char titleStr[40];
    sprintf(titleStr, "IR Remote [%s]", quickDevices[quickDevice]);
    u8g2.drawStr(15, 9, titleStr);
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 20, "Dev:");
    char devDisplay[50];
    sprintf(devDisplay, "< %s >", quickDevices[quickDevice]);
    u8g2.drawStr(35, 20, devDisplay);
    
    int startIdx = 0;
    if (quickSelection >= QUICK_VISIBLE_ITEMS) {
        startIdx = quickSelection - QUICK_VISIBLE_ITEMS + 1;
    }
    if (startIdx > quickMainCount - QUICK_VISIBLE_ITEMS) {
        startIdx = quickMainCount - QUICK_VISIBLE_ITEMS;
    }
    if (startIdx < 0) startIdx = 0;
    
    for (int i = 0; i < QUICK_VISIBLE_ITEMS && (startIdx + i) < quickMainCount; i++) {
        int idx = startIdx + i;
        int y = 28 + i * 9;
        
        if (idx == quickSelection) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y-1, 128, 10);
            u8g2.setDrawColor(1);
            u8g2.drawStr(2, y+1, ">");
            u8g2.drawStr(12, y+1, quickMainItems[idx]);
        } else {
            u8g2.setDrawColor(1);
            u8g2.drawStr(12, y+1, quickMainItems[idx]);
        }
    }
    
    if (startIdx > 0) u8g2.drawStr(120, 26, "^");
    if (startIdx + QUICK_VISIBLE_ITEMS < quickMainCount) u8g2.drawStr(120, 62, "v");
    
    u8g2.setCursor(0, 63);
    u8g2.print("UP/DN:Nav  SEL+UP/DN:Dev");
    
    u8g2.sendBuffer();
}

// =================================================
// QUICK REMOTE HANDLER - 4 BUTTONS
// =================================================

void handleQuickRemote() {
    drawQuickRemote();
    
    // UP/DOWN to navigate
    if (isButtonPressed(BTN_UP)) {
        quickSelection = (quickSelection - 1 + quickMainCount) % quickMainCount;
        drawQuickRemote();
        delay(150);
        return;
    }
    
    if (isButtonPressed(BTN_DOWN)) {
        quickSelection = (quickSelection + 1) % quickMainCount;
        drawQuickRemote();
        delay(150);
        return;
    }
    
    // SELECT + UP: Previous device
    if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_UP)) {
        quickDevice = (quickDevice - 1 + QUICK_DEVICE_COUNT) % QUICK_DEVICE_COUNT;
        if (quickDevice > 0) {
            loadDeviceCodes(quickDevices[quickDevice]);
        } else {
            clearActiveCodes();
        }
        drawQuickRemote();
        delay(200);
        return;
    }
    
    // SELECT + DOWN: Next device
    if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_DOWN)) {
        quickDevice = (quickDevice + 1) % QUICK_DEVICE_COUNT;
        if (quickDevice > 0) {
            loadDeviceCodes(quickDevices[quickDevice]);
        } else {
            clearActiveCodes();
        }
        drawQuickRemote();
        delay(200);
        return;
    }
    
    // SELECT (single press): Send code
    if (isButtonPressed(BTN_SELECT) && !isButtonPressed(BTN_UP) && !isButtonPressed(BTN_DOWN)) {
        if (quickSelection < irCodeCount) {
            uint32_t code = irCodes[quickSelection].code;
            if (code != 0) {
                sendIRCode(code);
                showIRMsg("Sent:", irCodes[quickSelection].name.c_str(), quickDevices[quickDevice]);
                delay(500);
                drawQuickRemote();
            }
        } else {
            showIRMsg("Not available", quickDevices[quickDevice], "");
            delay(300);
            drawQuickRemote();
        }
        return;
    }
    
    if (isButtonPressed(BTN_BACK)) {
        irState = IR_IDLE;
        drawIRMenu();
        return;
    }
    
    delay(20);
}

void startQuickRemote() {
    irState = IR_QUICK_CTRL;
    quickSelection = 0;
    quickScrollTop = 0;
    quickDevice = 0;
    drawQuickRemote();
}

// =================================================
// DRAW IR MENU
// =================================================

void drawIRMenu() {
    if (irState != IR_IDLE) return;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_ir);
    u8g2.drawStr(15, 9, "IR Tools");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    char cCnt[20];
    sprintf(cCnt, "Codes: %d", irCodeCount);
    u8g2.drawStr(0, 63, cCnt);
    
    if (wifiEnabled) u8g2.drawStr(100, 63, "📡");
    
    const char* items[] = {"Quick Remote", "Learn IR", "Send", "Random", "Blast", "Web UI", "Clear All", "< Back"};
    int count = 8;
    
    int startIdx = 0;
    if (irMenuIdx >= 5) startIdx = irMenuIdx - 5;
    if (startIdx > count - 6) startIdx = count - 6;
    if (startIdx < 0) startIdx = 0;
    
    for (int i = 0; i < 6 && (startIdx + i) < count; i++) {
        int idx = startIdx + i;
        int y = 18 + i * 8;
        if (idx == irMenuIdx) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y-1, 128, 9);
            u8g2.setDrawColor(1);
            u8g2.drawStr(2, y+1, ">");
            u8g2.drawStr(12, y+1, items[idx]);
        } else {
            u8g2.setDrawColor(1);
            u8g2.drawStr(12, y+1, items[idx]);
        }
    }
    
    u8g2.sendBuffer();
}

// =================================================
// LEARN FUNCTIONS
// =================================================

void startLearn() {
    if (!irInitialized) {
        irsend.begin();
        irrecv.enableIRIn();
        irInitialized = true;
    }
    irState = IR_LEARN;
    irLearnStart = millis();
    irrecv.resume();
    showIRMsg("Learning...", "Press remote key", "BACK cancel");
}

void processLearn() {
    if (isButtonPressed(BTN_BACK)) {
        irState = IR_IDLE;
        showIRMsg("Cancelled", "", "");
        delay(500);
        drawIRMenu();
        return;
    }
    
    if (irrecv.decode(&irResults)) {
        char c[20], h[16];
        sprintf(c, "Saved #%d", irCodeCount + 1);
        sprintf(h, "0x%08X", irResults.value);
        
        String device = "Unknown";
        uint32_t code = irResults.value;
        if ((code & 0xFFFF0000) == 0xE0E00000) device = "Samsung";
        else if ((code & 0xFFFF0000) == 0x20DF0000) device = "LG";
        else if (code < 0x1000) device = "Sony";
        else if ((code & 0xFFFF0000) == 0x00FF0000) device = "DD Free Dish";
        
        addIRCode(code, "Learned " + String(irCodeCount + 1), device, "Custom");
        
        showIRMsg(c, h, device.c_str());
        delay(800);
        irState = IR_IDLE;
        irrecv.resume();
        drawIRMenu();
        return;
    }
    
    if (millis() - irLearnStart > 15000) {
        showIRMsg("Timeout!", "No IR received", "");
        delay(1000);
        irState = IR_IDLE;
        drawIRMenu();
    }
}

// =================================================
// SEND FUNCTIONS
// =================================================

void startSend() {
    if (irCodeCount == 0) {
        showIRMsg("No codes!", "Learn first", "");
        delay(1000);
        drawIRMenu();
        return;
    }
    irState = IR_SEND;
    irSendIdx = 0;
}

void processSend() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 0, 16, 16, icon_ir);
    
    CenteredStr(12, "Send IR Code");
    char idxStr[20];
    sprintf(idxStr, "#%d/%d", irSendIdx + 1, irCodeCount);
    CenteredStr(25, idxStr);
    
    char hexStr[20];
    sprintf(hexStr, "0x%08X", irCodes[irSendIdx].code);
    CenteredStr(38, hexStr);
    
    String info = irCodes[irSendIdx].device + " - " + irCodes[irSendIdx].name;
    CenteredStr(50, info.c_str());
    
    u8g2.drawStr(0, 63, "UP/DN:Nav  SEL:Send  BACK:Exit");
    u8g2.sendBuffer();
    
    if (isButtonPressed(BTN_UP)) {
        irSendIdx = (irSendIdx + 1) % irCodeCount;
        delay(150);
    }
    if (isButtonPressed(BTN_DOWN)) {
        irSendIdx = (irSendIdx - 1 + irCodeCount) % irCodeCount;
        delay(150);
    }
    if (isButtonPressed(BTN_SELECT)) {
        if (!irInitialized) {
            irsend.begin();
            irInitialized = true;
        }
        irsend.sendNEC(irCodes[irSendIdx].code, 32);
        char h[20];
        sprintf(h, "Sent: 0x%08X", irCodes[irSendIdx].code);
        showIRMsg(h, irCodes[irSendIdx].name.c_str(), "");
        delay(600);
        irState = IR_IDLE;
        drawIRMenu();
        return;
    }
    if (isButtonPressed(BTN_BACK)) {
        irState = IR_IDLE;
        drawIRMenu();
    }
}

// =================================================
// RANDOM FUNCTIONS
// =================================================

void startRandom() {
    if (!irInitialized) {
        irsend.begin();
        irInitialized = true;
    }
    uint32_t c = random(0x10000000, 0xFFFFFFFF);
    irsend.sendNEC(c, 32);
    char h[20];
    sprintf(h, "0x%08X", c);
    showIRMsg("Random Sent!", h, "");
    delay(800);
    irState = IR_IDLE;
    drawIRMenu();
}

// =================================================
// BLAST FUNCTIONS
// =================================================

void startBlast() {
    if (!irInitialized) {
        irsend.begin();
        irInitialized = true;
    }
    irBlastCnt = 0;
    irState = IR_BLAST;
}

void processBlast() {
    if (irBlastCnt < 20) {
        uint32_t c = random(0x10000000, 0xFFFFFFFF);
        irsend.sendNEC(c, 32);
        
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawXBMP(56, 5, 16, 16, icon_ir);
        CenteredStr(20, "IR Blast Mode");
        
        char cntStr[20];
        sprintf(cntStr, "%d/20", irBlastCnt + 1);
        CenteredStr(35, cntStr);
        
        char hexStr[20];
        sprintf(hexStr, "0x%08X", c);
        CenteredStr(48, hexStr);
        
        int prog = map(irBlastCnt + 1, 0, 20, 0, 120);
        u8g2.drawFrame(4, 52, 120, 8);
        u8g2.drawBox(6, 54, prog, 4);
        u8g2.drawStr(0, 63, "BACK stop");
        u8g2.sendBuffer();
        
        irBlastCnt++;
        delay(300);
        
        if (isButtonPressed(BTN_BACK)) {
            irState = IR_IDLE;
            showIRMsg("Stopped!", "", "");
            delay(500);
            drawIRMenu();
            return;
        }
    } else {
        showIRMsg("Finished!", "20 codes sent", "");
        delay(1000);
        irState = IR_IDLE;
        drawIRMenu();
    }
}

// =================================================
// CLEAR IR CODES
// =================================================

void clearIRCodes() {
    irCodeCount = 0;
    if (SPIFFS.exists(IR_FILE)) {
        SPIFFS.remove(IR_FILE);
        Serial.println("🗑️ Removed IR file from SPIFFS");
    }
    showIRMsg("Cleared!", "All codes removed", "");
    delay(500);
    drawIRMenu();
}

// =================================================
// WEB INTERFACE
// =================================================

String getIRWebPage() {
    // ... (existing web page code - unchanged)
    // Keeping it short for brevity, but full code should be here
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>IR Code Manager</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0a0a0a;color:#e0e0e0;padding:20px}
.container{max-width:900px;margin:0 auto;background:#1a1a1a;border-radius:12px;padding:20px;border:1px solid #333}
h1{color:#00ff41;font-size:24px;margin-bottom:5px}
.subtitle{color:#888;font-size:13px;margin-bottom:20px}
.status-bar{display:flex;gap:15px;flex-wrap:wrap;margin-bottom:15px;background:#222;padding:10px 15px;border-radius:8px}
.status-bar span{font-size:13px}
.status-bar .value{color:#00ff41;font-weight:bold}
.search-box{width:100%;padding:10px;border-radius:6px;border:1px solid #333;background:#222;color:#fff;margin-bottom:15px}
.search-box:focus{outline:none;border-color:#00ff41}
.code-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:8px}
.code-item{background:#222;padding:10px 12px;border-radius:6px;border-left:3px solid #00ff41}
.code-item .name{font-weight:bold;font-size:14px;color:#00ff41}
.code-item .details{font-size:11px;color:#888}
.code-item .hex{font-family:monospace;font-size:12px;color:#ff6b6b}
.code-item .actions{display:flex;gap:6px;margin-top:6px;flex-wrap:wrap}
.code-item .actions button{padding:4px 12px;border:none;border-radius:4px;cursor:pointer;font-size:11px}
.btn-send{background:#00ff41;color:#000}
.btn-delete{background:#ff4444;color:#fff}
.btn-edit{background:#ffaa00;color:#000}
.btn-add{background:#00ff41;color:#000;border:none;padding:10px 20px;border-radius:6px;cursor:pointer;font-weight:bold;margin-top:10px}
.btn-add:hover{background:#00cc33}
.add-form{background:#222;padding:15px;border-radius:8px;margin-top:15px;display:none}
.add-form input{width:100%;padding:8px;margin:4px 0;border-radius:4px;border:1px solid #333;background:#111;color:#fff}
.add-form button{margin-top:8px}
</style>
</head>
<body>
<div class="container">
<h1>📡 IR Code Manager</h1>
<div class="subtitle">Manage your IR remote codes - Stored in SPIFFS</div>
<div class="status-bar">
<span>📊 Codes: <span class="value" id="codeCount">)rawliteral";
    html += String(irCodeCount);
    html += R"rawliteral(</span></span>
<span>💾 Max: <span class="value">)rawliteral";
    html += String(MAX_ACTIVE_CODES);
    html += R"rawliteral(</span></span>
</div>
<input class="search-box" id="searchInput" placeholder="🔍 Search by name, device, or function..." onkeyup="filterCodes()">
<button class="btn-add" onclick="toggleAddForm()">➕ Add New Code</button>
<div class="add-form" id="addForm">
    <h3 style="color:#00ff41;margin-bottom:8px;">Add IR Code</h3>
    <input type="text" id="newName" placeholder="Name (e.g., Power ON)">
    <input type="text" id="newDevice" placeholder="Device (e.g., Samsung TV)">
    <input type="text" id="newFunction" placeholder="Function (e.g., Power)">
    <input type="text" id="newCode" placeholder="Hex Code (e.g., 0xE0E09966)">
    <button class="btn-add" onclick="addCode()" style="margin-top:4px">💾 Save Code</button>
    <button onclick="toggleAddForm()" style="background:#444;color:#fff;border:none;padding:6px 12px;border-radius:4px;cursor:pointer;margin-left:6px">Cancel</button>
</div>
<div id="codeList" class="code-grid">
)rawliteral";

    if (irCodeCount > 0) {
        for (int i = 0; i < irCodeCount; i++) {
            char hexStr[16];
            sprintf(hexStr, "0x%08X", irCodes[i].code);
            
            html += "<div class='code-item' data-search='" + 
                    irCodes[i].name + " " + irCodes[i].device + " " + irCodes[i].function + "'>";
            html += "<div class='name'>" + irCodes[i].name + "</div>";
            html += "<div class='details'>" + irCodes[i].device + " → " + irCodes[i].function + "</div>";
            html += "<div class='hex'>" + String(hexStr) + "</div>";
            html += "<div class='actions'>";
            html += "<button class='btn-send' onclick='sendCode(" + String(i) + ")'>▶ Send</button>";
            html += "<button class='btn-edit' onclick='editCode(" + String(i) + ")'>✏️</button>";
            html += "<button class='btn-delete' onclick='deleteCode(" + String(i) + ")'>🗑️</button>";
            html += "</div></div>";
        }
    } else {
        html += "<div style='grid-column:1/-1;text-align:center;padding:40px;color:#666;'>";
        html += "📭 No IR codes stored yet.<br>Use 'Learn IR' on the device or add codes manually above.";
        html += "</div>";
    }

    html += R"rawliteral(
</div>
</div>
<script>
function sendCode(index) {
    fetch('/ir/send?index=' + index)
    .then(r => r.text())
    .then(data => { alert('✅ Code sent!'); });
}
function deleteCode(index) {
    if(confirm('Delete this code?')) {
        fetch('/ir/delete?index=' + index)
        .then(r => r.text())
        .then(() => { location.reload(); });
    }
}
function editCode(index) {
    let name = prompt('Enter new name:');
    if(name !== null) {
        fetch('/ir/edit?index=' + index + '&name=' + encodeURIComponent(name))
        .then(() => { location.reload(); });
    }
}
function addCode() {
    let name = document.getElementById('newName').value;
    let device = document.getElementById('newDevice').value;
    let functionVal = document.getElementById('newFunction').value;
    let code = document.getElementById('newCode').value;
    if(!name || !code) { alert('Name and Code are required!'); return; }
    fetch('/ir/add?name=' + encodeURIComponent(name) + '&device=' + encodeURIComponent(device) + 
          '&function=' + encodeURIComponent(functionVal) + '&code=' + encodeURIComponent(code))
    .then(r => r.text())
    .then(() => { location.reload(); });
}
function toggleAddForm() {
    let form = document.getElementById('addForm');
    form.style.display = form.style.display === 'none' ? 'block' : 'none';
}
function filterCodes() {
    let input = document.getElementById('searchInput');
    let filter = input.value.toLowerCase();
    let items = document.getElementsByClassName('code-item');
    for(let i = 0; i < items.length; i++) {
        let search = items[i].getAttribute('data-search').toLowerCase();
        items[i].style.display = search.indexOf(filter) > -1 ? 'flex' : 'none';
    }
}
</script>
</body>
</html>
)rawliteral";
    return html;
}

// =================================================
// WEB SERVER SETUP
// =================================================

void setupIRWebServer() {
    webServer.on("/ir", []() {
        webServer.send(200, "text/html", getIRWebPage());
    });
    
    webServer.on("/ir/send", []() {
        if (webServer.hasArg("index")) {
            int index = webServer.arg("index").toInt();
            if (index >= 0 && index < irCodeCount) {
                irsend.sendNEC(irCodes[index].code, 32);
                webServer.send(200, "text/plain", "Sent");
            } else {
                webServer.send(400, "text/plain", "Invalid index");
            }
        } else {
            webServer.send(400, "text/plain", "Missing index");
        }
    });
    
    webServer.on("/ir/delete", []() {
        if (webServer.hasArg("index")) {
            int index = webServer.arg("index").toInt();
            if (index >= 0 && index < irCodeCount) {
                for (int i = index; i < irCodeCount - 1; i++) {
                    irCodes[i] = irCodes[i + 1];
                }
                irCodeCount--;
                saveIRCodes();
                webServer.send(200, "text/plain", "Deleted");
            } else {
                webServer.send(400, "text/plain", "Invalid index");
            }
        } else {
            webServer.send(400, "text/plain", "Missing index");
        }
    });
    
    webServer.on("/ir/edit", []() {
        if (webServer.hasArg("index") && webServer.hasArg("name")) {
            int index = webServer.arg("index").toInt();
            String name = webServer.arg("name");
            if (index >= 0 && index < irCodeCount) {
                irCodes[index].name = name;
                saveIRCodes();
                webServer.send(200, "text/plain", "Edited");
            } else {
                webServer.send(400, "text/plain", "Invalid index");
            }
        } else {
            webServer.send(400, "text/plain", "Missing parameters");
        }
    });
    
    webServer.on("/ir/add", []() {
        if (webServer.hasArg("name") && webServer.hasArg("code")) {
            String name = webServer.arg("name");
            String device = webServer.hasArg("device") ? webServer.arg("device") : "Custom";
            String function = webServer.hasArg("function") ? webServer.arg("function") : "Custom";
            String codeStr = webServer.arg("code");
            uint32_t code = strtoul(codeStr.c_str(), NULL, 16);
            addIRCode(code, name, device, function);
            webServer.send(200, "text/plain", "Added");
        } else {
            webServer.send(400, "text/plain", "Missing parameters");
        }
    });
    
    webServer.on("/ir/export", []() {
        String json = "{\"count\":" + String(irCodeCount) + ",\"codes\":[";
        for (int i = 0; i < irCodeCount; i++) {
            if (i > 0) json += ",";
            char hexStr[16];
            sprintf(hexStr, "0x%08X", irCodes[i].code);
            json += "{";
            json += "\"code\":" + String(irCodes[i].code) + ",";
            json += "\"hex\":\"" + String(hexStr) + "\",";
            json += "\"name\":\"" + irCodes[i].name + "\",";
            json += "\"device\":\"" + irCodes[i].device + "\",";
            json += "\"function\":\"" + irCodes[i].function + "\",";
            json += "\"active\":" + String(irCodes[i].active);
            json += "}";
        }
        json += "]}";
        webServer.send(200, "application/json", json);
    });
    
    webServer.on("/ir/", []() {
        webServer.sendHeader("Location", "/ir", true);
        webServer.send(302, "text/plain", "");
    });
    
    Serial.println("🌐 IR Web Server routes configured");
}

// =================================================
// START WEB UI
// =================================================

void startWebUI() {
    if (!wifiEnabled) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-IR", "esp32remote");
        wifiEnabled = true;
        setupIRWebServer();
        Serial.println("📡 Web server started at: " + WiFi.softAPIP().toString());
    }
    showIRMsg("Web Ready!", "Connect to ESP32-IR", WiFi.softAPIP().toString().c_str());
    delay(2000);
    drawIRMenu();
}

// =================================================
// SETUP AND LOOP
// =================================================

void irSetup() {
    Serial.println("\n🔴 IR TOOLS INITIALIZING");
    Serial.println("================================");
    
    loadIRCodes();
    
    if (!irInitialized) {
        irsend.begin();
        irrecv.enableIRIn();
        irInitialized = true;
        Serial.println("✅ IR hardware initialized");
    }
    
    irState = IR_IDLE;
    irMenuIdx = 0;
    drawIRMenu();
    
    Serial.printf("📊 Loaded %d IR codes in RAM\n", irCodeCount);
    Serial.println("================================");
}

void irLoop() {
    if (irState == IR_QUICK_CTRL) {
        handleQuickRemote();
        return;
    }
    if (irState == IR_LEARN) {
        processLearn();
        return;
    }
    if (irState == IR_SEND) {
        processSend();
        return;
    }
    if (irState == IR_BLAST) {
        processBlast();
        return;
    }
    
    if (isButtonPressed(BTN_BACK)) {
        if (wifiEnabled) {
            webServer.stop();
            wifiEnabled = false;
            WiFi.softAPdisconnect(true);
        }
        exitModule();
        return;
    }
    
    if (isButtonPressed(BTN_UP)) {
        irMenuIdx = (irMenuIdx - 1 + 8) % 8;
        drawIRMenu();
        delay(150);
    }
    
    if (isButtonPressed(BTN_DOWN)) {
        irMenuIdx = (irMenuIdx + 1) % 8;
        drawIRMenu();
        delay(150);
    }
    
    if (isButtonPressed(BTN_SELECT)) {
        switch (irMenuIdx) {
            case 0: startQuickRemote(); break;
            case 1: startLearn(); break;
            case 2: startSend(); break;
            case 3: startRandom(); break;
            case 4: startBlast(); break;
            case 5: startWebUI(); break;
            case 6: clearIRCodes(); break;
            case 7:
                if (wifiEnabled) {
                    webServer.stop();
                    wifiEnabled = false;
                    WiFi.softAPdisconnect(true);
                }
                exitModule();
                return;
        }
    }
    
    if (wifiEnabled) {
        webServer.handleClient();
    }
    
    delay(20);
}

// =================================================
// CLEANUP
// =================================================
// Add this function or update the existing one
void irCleanup() {
    Serial.println("🧹 IR Tools cleanup...");
    unsigned long startTime = millis();
    
    if (irState != IR_IDLE) {
        irState = IR_IDLE;
        delay(50);
    }
    
    if (wifiEnabled) {
        webServer.stop();
        wifiEnabled = false;
        WiFi.softAPdisconnect(true);
        delay(100);
    }
    
    if (irInitialized) {
        irrecv.disableIRIn();
        irInitialized = false;
    }
    
    irMenuIdx = 0;
    irSendIdx = 0;
    irBlastCnt = 0;
    irCodeCount = 0;
    
    WiFi.mode(WIFI_OFF);
    delay(100);
    clearPopup();
    
    Serial.printf("✅ IR Tools cleanup complete (%lu ms)\n", millis() - startTime);
}