#ifndef DEAUTHER_H
#define DEAUTHER_H

#include <Arduino.h>

void deautherSetup();
void deautherLoop();
void deautherCleanup();  // ✅ NEW: Cleanup function

#endif