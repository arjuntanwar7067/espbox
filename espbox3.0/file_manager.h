#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>

void fileManagerSetup();
void fileManagerLoop();
void fileManagerCleanup();  // ✅ NEW: Cleanup function

#endif