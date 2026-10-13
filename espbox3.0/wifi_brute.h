#ifndef WIFI_BRUTE_H
#define WIFI_BRUTE_H

#include <Arduino.h>

void wifiBruteSetup();
void wifiBruteLoop();
void wifiBruteCleanup();  // ✅ NEW: Cleanup function

#endif