#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

void otaSetup();
void otaLoop();
void otaCleanup();

// WiFi credentials storage
struct WiFiCreds {
    char ssid[32];
    char password[64];
    bool valid;
};

extern WiFiCreds savedWiFi;
extern bool otaActive;
extern String otaStatus;
extern float otaProgress;

void loadWiFiCredentials();
void saveWiFiCredentials(const char* ssid, const char* password);
void clearWiFiCredentials();
bool connectToWiFi();

#endif