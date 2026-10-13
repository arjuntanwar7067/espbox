#ifndef WIFI_BEACON_FLOOD_H
#define WIFI_BEACON_FLOOD_H

#include <Arduino.h>

void beaconFloodSetup();
void beaconFloodLoop();
void beaconFloodCleanup();  // ✅ NEW: Cleanup function

#endif