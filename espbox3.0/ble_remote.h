#ifndef BLE_REMOTE_H
#define BLE_REMOTE_H

#include <Arduino.h>

void bleRemoteSetup();
void bleRemoteLoop();
void bleRemoteCleanup();  // ✅ NEW: Cleanup function

#endif