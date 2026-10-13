#ifndef IR_TOOLS_H
#define IR_TOOLS_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

#define MAX_ACTIVE_CODES 30
#define IR_FILE "/ir_codes.json"

struct IRCode {
    uint32_t code;
    String name;
    String device;
    String function;
    bool active;
};

// Function Declarations
void irSetup();
void irLoop();
void irCleanup();
void loadIRCodes();
void saveIRCodes();
void addIRCode(uint32_t code, String name = "", String device = "", String function = "");
void preloadDefaultCodes();
void clearIRCodes();
void startLearn();
void processLearn();
void startSend();
void processSend();
void startRandom();
void startBlast();
void processBlast();
void showIRMsg(const char* l1, const char* l2 = "", const char* l3 = "");
String getProtocolName(decode_results *results);
void startQuickRemote();
void handleQuickRemote();
void drawQuickRemote();

#endif