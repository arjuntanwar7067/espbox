#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "config.h"
#include "ota.h"

enum ModuleState {
    MODULE_UNINITIALIZED = 0,
    MODULE_INITIALIZED,
    MODULE_RUNNING,
    MODULE_PAUSED,
    MODULE_STOPPING
};

struct MenuItem {
    char name[MAX_ITEM_LENGTH];
    int iconIndex;
    void (*setupFunc)();
    void (*loopFunc)();
    void (*cleanupFunc)();
    void (*pauseFunc)();
    void (*resumeFunc)();
    ModuleState state;
    bool isRunning;
    bool needsCleanup;
    uint32_t entryTime;
    uint32_t exitTime;
};

// Main menu functions
void drawMenu();
void drawScrollingMenu(const char** items, int itemCount, int selected, int maxVisible, const char* title, int iconIndex);

// UI helper functions
void drawRoundedBox(int x, int y, int w, int h, int r);
void drawFilledRoundedBox(int x, int y, int w, int h, int r);

// Menu management
void addMenuItem(const char* name, int iconIdx, void (*setup)(), void (*loop)(), void (*cleanup)() = nullptr);
void initMenu();
void enterModule(int index);
void exitModule();
void emergencyExitModule();

// Bruce icon drawing
void drawBruceIcon(int iconIdx, int x, int y);

// External variables
extern MenuItem menuItems[MAX_MENU_ITEMS];
extern int itemCount;
extern int selectedItem;
extern int currentScreen;
extern String currentTimeStr;

#endif