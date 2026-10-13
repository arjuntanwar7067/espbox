#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// =================================================
// EXTERN - Defined in utils.cpp
// =================================================
extern const int buttonPins[4];
extern unsigned long lastPress[4];

// =================================================
// BUTTON FUNCTIONS
// =================================================
bool isButtonPressed(int pin);

// =================================================
// DISPLAY FUNCTIONS
// =================================================
void CenteredStr(uint8_t y, const char* text);

// =================================================
// TIME FUNCTIONS
// =================================================
String getTimestampStr();
void updateTimeDisplay();
void drawClockScreen();

// =================================================
// WIFI FUNCTIONS
// =================================================
void disableWiFi();
void enableWiFi();

// =================================================
// STORAGE INFO FUNCTIONS
// =================================================
void printStorageInfo();
String getStorageInfoString();
void drawStorageInfoOLED();

// =================================================
// UNIFIED POPUP SYSTEM
// =================================================
void showPopupMessage(const char* line1, const char* line2 = "", const char* line3 = "");
void showCredentialPopup(const char* username, const char* password, const char* ip, const char* type);
bool drawPopupIfActive();
void clearPopup();

// Popup state variables
extern bool g_showPopup;
extern bool g_showCredPopup;
extern String g_popupLine1;
extern String g_popupLine2;
extern String g_popupLine3;
extern String g_lastCredUsername;
extern String g_lastCredPassword;
extern String g_lastCredIP;
extern String g_lastCredType;
extern unsigned long g_popupStartTime;
extern const unsigned long POPUP_DURATION;
extern const unsigned long CRED_POPUP_DURATION;

// Time variables
extern String currentTimeStr;
extern String currentHours;
extern String currentMinutes;
extern String currentSeconds;
extern bool showClockScreen;
extern bool menuLoaded;

#endif