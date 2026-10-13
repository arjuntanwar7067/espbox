#ifndef ROGUE_AP_H
#define ROGUE_AP_H

#include <Arduino.h>
#include <WebServer.h>
#include "credentials.h"

enum PhishingTemplate {
    PHISH_INSTAGRAM = 0,
    PHISH_GOOGLE = 1,
    PHISH_FACEBOOK = 2,
    PHISH_MICROSOFT = 3,
    PHISH_TWITTER = 4,
    PHISH_TIKTOK = 5,
    PHISH_CUSTOM = 6
};

struct RogueAPConfig {
    String ssid;
    String wpaPassword;
    PhishingTemplate templateType;
    int channel;
    bool useClonedMAC;
    bool preserveOriginal;
    bool enableWPA2;
    bool enableKarma;
    bool enableProbeResponse;
    bool useEvilTwin;
    bool enableDeauth;
    
    RogueAPConfig() {
        ssid = "Free_WiFi";
        wpaPassword = "";
        templateType = PHISH_INSTAGRAM;
        channel = 6;
        useClonedMAC = false;
        preserveOriginal = false;
        enableWPA2 = false;
        enableKarma = false;
        enableProbeResponse = false;
        useEvilTwin = false;
        enableDeauth = false;
    }
};

// ✅ NEW: Dedicated web server for Rogue AP
extern WebServer rogueWebServer;

// External variables
extern RogueAPConfig currentConfig;
extern bool rogueAPActive;
extern int rogueMenu;
extern String selectedSSID;
extern bool inScanMenu;
extern int scanMenuOffset;
extern const char* pageNames[7];

// Function declarations
void rogueAPSetup();
void rogueAPLoop();
void rogueAPCleanup();
void startRogueAP(RogueAPConfig config);
void stopRogueAP();
void scanNetworks();
void selectTargetNetwork(String ssid);
void cloneTargetNetwork();
void enableEvilTwin(String targetSSID);
void enableKarmaAttack();

#endif