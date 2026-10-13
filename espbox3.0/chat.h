#ifndef CHAT_H
#define CHAT_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include "config.h"

#define MAX_PRIVATE_CHATS 50

enum ChatMode {
    CHAT_MODE_GROUP = 0,
    CHAT_MODE_PRIVATE = 1
};

struct ChatMessage {
    char username[MAX_USERNAME];
    char message[MAX_MESSAGE];
    char time[9];
    bool used;
};

struct ChatClient {
    char username[MAX_USERNAME];
    uint8_t wsNum;
    bool active;
    char ip[16];
    uint32_t lastSeen;
    bool isAdmin;
};

struct PrivateMessage {
    char from[MAX_USERNAME];
    char to[MAX_USERNAME];
    char message[MAX_MESSAGE];
    char time[9];
    bool read;
};

// External declarations
extern ChatMessage chatMessages[MAX_MESSAGES];
extern ChatClient chatClients[MAX_CLIENTS];
extern PrivateMessage privateMessages[MAX_PRIVATE_CHATS];
extern int messageCount;
extern int clientCount;
extern int privateMsgCount;
extern String chatSSID;
extern bool chatActive;
extern String adminUsername;
extern ChatMode currentChatMode;

// Function declarations
void chatSetup();
void chatLoop();
void chatCleanup();
void startChatAP(String ssid);
void stopChatAP();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void sendPrivateMessage(String from, String to, String message);
void broadcastGroupMessage(String username, String message);
void updateUserList();
String getTimeStr();

#endif