#include "ble_remote.h"
#include "config.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <BleCombo.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// ============================================================
// BLE COMBO OBJECT
// ============================================================

BleCombo bleCombo("ESP32 Remote");

// ============================================================
// SYSTEM MODES
// ============================================================

enum SystemMode {
    MODE_MENU,
    MODE_AIR_MOUSE,
    MODE_KEYBOARD
};

// ============================================================
// BUTTON STATE
// ============================================================

bool lastButtonState[4] = {HIGH, HIGH, HIGH, HIGH};
bool buttonState[4] = {HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
const unsigned long debounceDelay = 50;
unsigned long buttonPressStart[4] = {0, 0, 0, 0};
bool buttonHeld[4] = {false, false, false, false};

SystemMode currentMode = MODE_MENU;

// ============================================================
// MENU STATES
// ============================================================

enum MenuState {
    MAIN_MENU,
    MOUSE_MENU,
    KEYBOARD_MENU,
    MEDIA_MENU,
    APP_MENU,
    CAMERA_MENU,
    SYSTEM_MENU,
    SETTINGS_MENU,
    SENSITIVITY_MENU,
    UNLOCK_MENU
};

MenuState currentMenu = MAIN_MENU;
int currentSelection = 0;
int menuTop = 0;
const int VISIBLE_ITEMS = 4;

// ============================================================
// MENU ITEMS
// ============================================================

const char* mainMenuItems[] = {
    "Mouse",
    "Keyboard",
    "Media",
    "Applications",
    "Camera",
    "System",
    "Unlock Laptop",
    "Settings"
};
const int mainMenuLength = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);

const char* mouseItems[] = {
    "Air Mouse",
    "Move Right",
    "Move Left",
    "Move Up",
    "Move Down",
    "Left Click",
    "Right Click",
    "Middle Click",
    "Scroll Up",
    "Scroll Down",
    "Drag Mode"
};
const int mouseLength = sizeof(mouseItems) / sizeof(mouseItems[0]);

const char* keyboardItems[] = {
    "Keyboard Mode",
    "Copy",
    "Paste",
    "Cut",
    "Undo",
    "Redo",
    "Alt Tab",
    "Win D",
    "Win E",
    "Win R",
    "Screenshot",
    "Task Manager"
};
const int keyboardLength = sizeof(keyboardItems) / sizeof(keyboardItems[0]);

const char* mediaItems[] = {
    "Play/Pause",
    "Next Track",
    "Previous Track",
    "Volume Up",
    "Volume Down",
    "Mute",
    "Stop"
};
const int mediaLength = sizeof(mediaItems) / sizeof(mediaItems[0]);

const char* appItems[] = {
    "Chrome",
    "Brave",
    "VLC",
    "Calculator",
    "File Explorer",
    "Task Manager"
};
const int appLength = sizeof(appItems) / sizeof(appItems[0]);

const char* cameraItems[] = {
    "Windows Camera",
    "OBS",
    "Screenshot",
    "Start/Stop Video"
};
const int cameraLength = sizeof(cameraItems) / sizeof(cameraItems[0]);

const char* systemItems[] = {
    "Lock PC",
    "Sleep",
    "Restart",
    "Shutdown",
    "Desktop",
    "Volume Up",
    "Volume Down",
    "Mute"
};
const int systemLength = sizeof(systemItems) / sizeof(systemItems[0]);

const char* bleSettingsItems[] = {
    "Sensitivity",
    "MPU6050 Status",
    "Bluetooth Status",
    "OLED Refresh"
};
const int bleSettingsLength = sizeof(bleSettingsItems) / sizeof(bleSettingsItems[0]);

const char* sensitivityItems[] = {
    "Very Slow",
    "Slow",
    "Medium",
    "Fast"
};
const int sensitivityLength = sizeof(sensitivityItems) / sizeof(sensitivityItems[0]);

int sensitivity = 1;

const char* unlockPassword = "7067";

// ============================================================
// AIR MOUSE STATE
// ============================================================

bool dragMode = false;
int cursorSpeedValues[] = {4, 8, 14, 20};
bool movingUp = false;
bool movingDown = false;
bool movingLeft = false;
bool movingRight = false;
unsigned long lastMoveUpdate = 0;
const unsigned long moveInterval = 15;

unsigned long lastKeyPress = 0;
const unsigned long keyRepeatDelay = 150;
const unsigned long keyRepeatInterval = 50;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================

void displayMenu();
void displayAirMouse();
void displayKeyboardMode();
void displayUnlockMenu();
void handleButtons();
void handleSingleButton(int pin, int index);
void handleButtonPress(int index);
void handleLongPress(int index);
void handleMenuPress(int index);
void handleAirMousePress(int index);
void handleAirMouseRelease(int index);
void handleKeyboardPress(int index);
void moveUp();
void moveDown();
void goBack();
void selectItem();
void openMenu(MenuState menu);
void enterAirMouse();
void exitMode();
void enterKeyboardMode();
void exitKeyboardMode();
void updateAirMouse();
void handleKeyboardNavigation();
void updateMenuScroll();
int getCurrentMenuLength();
void triggerMouse(int selection);
void triggerKeyboard(int selection);
void triggerMedia(int selection);
void triggerApplication(int selection);
void triggerCamera(int selection);
void triggerSystem(int selection);
void triggerUnlock();
void runCommand(const char* command);
void showMPUStatus();
void showBluetoothStatus();

// ============================================================
// START BLE ADVERTISING
// ============================================================

void startBLEAdvertising() {
    // BleCombo handles advertising automatically via begin()
    if (!bleCombo.isConnected()) {
        bleCombo.begin();
        delay(100);
        Serial.println("✅ BLE Advertising started (BleCombo)");
        Serial.println("📡 Device Name: ESP32 Remote");
        Serial.println("🔍 Visible as: ESP32 Remote");
        Serial.println("📱 On PC: Add Bluetooth Device -> Search for 'ESP32 Remote'");
    }
}

// ============================================================
// BLE SETUP
// ============================================================

void bleRemoteSetup() {
    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 BLE REMOTE (BleCombo)");
    Serial.println("================================");

    currentMode = MODE_MENU;
    currentMenu = MAIN_MENU;
    currentSelection = 0;
    menuTop = 0;
    
    // Initialize BleCombo
    bleCombo.begin();
    delay(300);
    
    clearPopup();
    
    showPopupMessage("BLE Remote", "Pair with", "ESP32 Remote");
    delay(1500);
    displayMenu();
}

// ============================================================
// BLE LOOP
// ============================================================

void bleRemoteLoop() {
    if (drawPopupIfActive()) {
        delay(20);
        return;
    }

    handleButtons();

    switch (currentMode) {
        case MODE_AIR_MOUSE:
            updateAirMouse();
            break;
        case MODE_KEYBOARD:
            handleKeyboardNavigation();
            break;
        case MODE_MENU:
        default:
            break;
    }

    delay(1);
}

// ============================================================
// BUTTON HANDLER
// ============================================================

void handleButtons() {
    handleSingleButton(BTN_SELECT, 0);
    handleSingleButton(BTN_UP, 1);
    handleSingleButton(BTN_DOWN, 2);
    handleSingleButton(BTN_BACK, 3);
}

void handleSingleButton(int pin, int index) {
    bool reading = (digitalRead(pin) == LOW);

    if (reading != lastButtonState[index]) {
        lastDebounceTime[index] = millis();
    }

    if ((millis() - lastDebounceTime[index]) > debounceDelay) {
        if (reading != buttonState[index]) {
            buttonState[index] = reading;

            if (buttonState[index] == LOW) {
                buttonPressStart[index] = millis();
                buttonHeld[index] = true;
                handleButtonPress(index);
            } else {
                unsigned long duration = millis() - buttonPressStart[index];
                buttonHeld[index] = false;

                if (currentMode == MODE_AIR_MOUSE) {
                    handleAirMouseRelease(index);
                }

                if (duration >= 800) {
                    handleLongPress(index);
                }
            }
        }
    }

    lastButtonState[index] = reading;
}

void handleButtonPress(int index) {
    switch (currentMode) {
        case MODE_MENU:
            handleMenuPress(index);
            break;
        case MODE_AIR_MOUSE:
            handleAirMousePress(index);
            break;
        case MODE_KEYBOARD:
            handleKeyboardPress(index);
            break;
    }
}

void handleLongPress(int index) {
    if (index == 3) { // BACK button
        if (currentMode == MODE_AIR_MOUSE || currentMode == MODE_KEYBOARD) {
            exitMode();
        } else {
            currentMenu = MAIN_MENU;
            currentSelection = 0;
            menuTop = 0;
            displayMenu();
        }
    }
}

void handleMenuPress(int index) {
    switch (index) {
        case 0: selectItem(); break;
        case 1: moveUp(); break;
        case 2: moveDown(); break;
        case 3: goBack(); break;
    }
}

void handleAirMousePress(int index) {
    if (!bleCombo.isConnected()) {
        showPopupMessage("Disconnected!", "", "");
        delay(600);
        displayAirMouse();
        return;
    }

    switch (index) {
        case 1: // UP
            movingUp = true;
            break;
        case 2: // DOWN
            movingDown = true;
            break;
        case 0: // SELECT
            if (dragMode) {
                bleCombo.press(MOUSE_LEFT);
                Serial.println("Mouse: Drag Start");
            } else {
                bleCombo.click(MOUSE_LEFT);
                Serial.println("Mouse: Left Click");
            }
            break;
        case 3: // BACK
            exitMode();
            break;
    }
}

void handleAirMouseRelease(int index) {
    if (index == 1) movingUp = false;
    if (index == 2) movingDown = false;
    if (index == 0 && dragMode) {
        bleCombo.release(MOUSE_LEFT);
        Serial.println("Mouse: Drag Stop");
    }
}

void enterAirMouse() {
    Serial.println("Entering Air Mouse Mode");
    currentMode = MODE_AIR_MOUSE;
    movingUp = false;
    movingDown = false;
    movingLeft = false;
    movingRight = false;
    lastMoveUpdate = millis();

    showPopupMessage("Air Mouse", "UP/DN: Move", "SEL: Click");
    delay(800);
    displayAirMouse();
}

void exitMode() {
    Serial.println("Exiting Mode");
    if (dragMode && bleCombo.isConnected()) {
        bleCombo.release(MOUSE_LEFT);
        dragMode = false;
    }
    currentMode = MODE_MENU;
    currentMenu = MAIN_MENU;
    currentSelection = 0;
    menuTop = 0;
    displayMenu();
}

void handleKeyboardPress(int index) {
    if (!bleCombo.isConnected()) {
        showPopupMessage("Disconnected!", "", "");
        delay(600);
        displayKeyboardMode();
        return;
    }

    switch (index) {
        case 1: // UP
            bleCombo.write(KEY_UP_ARROW);
            Serial.println("Keyboard: Up");
            break;
        case 2: // DOWN
            bleCombo.write(KEY_DOWN_ARROW);
            Serial.println("Keyboard: Down");
            break;
        case 0: // SELECT
            bleCombo.write(KEY_RETURN);
            Serial.println("Keyboard: Enter");
            break;
        case 3: // BACK
            exitMode();
            break;
    }
}

void handleKeyboardNavigation() {
    if (currentMode != MODE_KEYBOARD) return;
    if (!bleCombo.isConnected()) return;

    unsigned long now = millis();
    if (now - lastKeyPress < keyRepeatDelay) return;

    for (int i = 1; i <= 2; i++) {
        if (buttonHeld[i]) {
            handleKeyboardPress(i);
            lastKeyPress = now;
            break;
        }
    }
}

void enterKeyboardMode() {
    Serial.println("Entering Keyboard Mode");
    currentMode = MODE_KEYBOARD;
    lastKeyPress = millis();
    showPopupMessage("Keyboard Mode", "UP/DN: Arrows", "SELECT: Enter");
    delay(800);
    displayKeyboardMode();
}

void exitKeyboardMode() {
    Serial.println("Exiting Keyboard Mode");
    currentMode = MODE_MENU;
    currentMenu = MAIN_MENU;
    currentSelection = 0;
    menuTop = 0;
    displayMenu();
}

void updateAirMouse() {
    if (currentMode != MODE_AIR_MOUSE) return;
    if (!bleCombo.isConnected()) return;

    if (millis() - lastMoveUpdate < moveInterval) return;
    lastMoveUpdate = millis();

    int moveX = 0;
    int moveY = 0;
    int speed = cursorSpeedValues[sensitivity];

    if (movingRight) moveX += speed;
    if (movingLeft) moveX -= speed;
    if (movingDown) moveY += speed;
    if (movingUp) moveY -= speed;

    if (moveX != 0 || moveY != 0) {
        bleCombo.move(moveX, moveY);
    }
}

void moveUp() {
    int length = getCurrentMenuLength();
    if (length <= 0) return;

    currentSelection = (currentSelection - 1 + length) % length;
    updateMenuScroll();
    displayMenu();
}

void moveDown() {
    int length = getCurrentMenuLength();
    if (length <= 0) return;

    currentSelection = (currentSelection + 1) % length;
    updateMenuScroll();
    displayMenu();
}

void goBack() {
    if (currentMenu != MAIN_MENU) {
        currentMenu = MAIN_MENU;
        currentSelection = 0;
        menuTop = 0;
        displayMenu();
    }
}

int getCurrentMenuLength() {
    switch (currentMenu) {
        case MAIN_MENU: return mainMenuLength;
        case MOUSE_MENU: return mouseLength;
        case KEYBOARD_MENU: return keyboardLength;
        case MEDIA_MENU: return mediaLength;
        case APP_MENU: return appLength;
        case CAMERA_MENU: return cameraLength;
        case SYSTEM_MENU: return systemLength;
        case SETTINGS_MENU: return bleSettingsLength;
        case SENSITIVITY_MENU: return sensitivityLength;
        case UNLOCK_MENU: return 1;
        default: return 0;
    }
}

void updateMenuScroll() {
    int len = getCurrentMenuLength();
    if (len <= 0) return;
    
    const int centerPos = 1;
    menuTop = currentSelection - centerPos;
    
    if (menuTop < 0) {
        menuTop = 0;
    } else if (menuTop + VISIBLE_ITEMS > len) {
        menuTop = len - VISIBLE_ITEMS;
        if (menuTop < 0) menuTop = 0;
    }
}

void selectItem() {
    switch (currentMenu) {
        case MAIN_MENU:
            switch (currentSelection) {
                case 0: openMenu(MOUSE_MENU); break;
                case 1: openMenu(KEYBOARD_MENU); break;
                case 2: openMenu(MEDIA_MENU); break;
                case 3: openMenu(APP_MENU); break;
                case 4: openMenu(CAMERA_MENU); break;
                case 5: openMenu(SYSTEM_MENU); break;
                case 6: openMenu(UNLOCK_MENU); break;
                case 7: openMenu(SETTINGS_MENU); break;
            }
            break;
        case MOUSE_MENU:
            triggerMouse(currentSelection);
            break;
        case KEYBOARD_MENU:
            triggerKeyboard(currentSelection);
            break;
        case MEDIA_MENU:
            triggerMedia(currentSelection);
            break;
        case APP_MENU:
            triggerApplication(currentSelection);
            break;
        case CAMERA_MENU:
            triggerCamera(currentSelection);
            break;
        case SYSTEM_MENU:
            triggerSystem(currentSelection);
            break;
        case SETTINGS_MENU:
            if (currentSelection == 0) {
                openMenu(SENSITIVITY_MENU);
            } else if (currentSelection == 1) {
                showMPUStatus();
            } else if (currentSelection == 2) {
                showBluetoothStatus();
            } else if (currentSelection == 3) {
                displayMenu();
            }
            break;
        case SENSITIVITY_MENU:
            sensitivity = currentSelection;
            currentMenu = SETTINGS_MENU;
            currentSelection = 0;
            menuTop = 0;
            displayMenu();
            break;
        case UNLOCK_MENU:
            triggerUnlock();
            break;
    }
}

void openMenu(MenuState menu) {
    currentMenu = menu;
    currentSelection = 0;
    menuTop = 0;
    displayMenu();
}

void triggerKeyboard(int selection) {
    if (selection == 0) {
        enterKeyboardMode();
        return;
    }
    if (!bleCombo.isConnected()) {
        showPopupMessage("Not Connected!", "", "");
        delay(500);
        displayMenu();
        return;
    }
    
    switch (selection) {
        case 1: // Copy
            bleCombo.press(KEY_LEFT_CTRL);
            bleCombo.write('c');
            bleCombo.releaseAll();
            break;
        case 2: // Paste
            bleCombo.press(KEY_LEFT_CTRL);
            bleCombo.write('v');
            bleCombo.releaseAll();
            break;
        case 3: // Cut
            bleCombo.press(KEY_LEFT_CTRL);
            bleCombo.write('x');
            bleCombo.releaseAll();
            break;
        case 4: // Undo
            bleCombo.press(KEY_LEFT_CTRL);
            bleCombo.write('z');
            bleCombo.releaseAll();
            break;
        case 5: // Redo
            bleCombo.press(KEY_LEFT_CTRL);
            bleCombo.write('y');
            bleCombo.releaseAll();
            break;
        case 6: // Alt Tab
            bleCombo.press(KEY_LEFT_ALT);
            bleCombo.press(KEY_TAB);
            bleCombo.releaseAll();
            break;
        case 7: // Win D
            bleCombo.press(KEY_LEFT_GUI);
            bleCombo.write('d');
            bleCombo.releaseAll();
            break;
        case 8: // Win E
            bleCombo.press(KEY_LEFT_GUI);
            bleCombo.write('e');
            bleCombo.releaseAll();
            break;
        case 9: // Win R
            bleCombo.press(KEY_LEFT_GUI);
            bleCombo.write('r');
            bleCombo.releaseAll();
            break;
        case 10: // Screenshot
            bleCombo.press(KEY_LEFT_GUI);
            bleCombo.write(KEY_PRTSC);
            bleCombo.releaseAll();
            break;
        case 11: // Task Manager
            bleCombo.press(KEY_LEFT_CTRL);
            bleCombo.press(KEY_LEFT_SHIFT);
            bleCombo.write(KEY_ESC);
            bleCombo.releaseAll();
            break;
    }
}

void triggerMedia(int selection) {
    if (!bleCombo.isConnected()) {
        showPopupMessage("Not Connected!", "", "");
        delay(500);
        displayMenu();
        return;
    }
    switch (selection) {
        case 0: bleCombo.write(KEY_MEDIA_PLAY_PAUSE); break;
        case 1: bleCombo.write(KEY_MEDIA_NEXT_TRACK); break;
        case 2: bleCombo.write(KEY_MEDIA_PREVIOUS_TRACK); break;
        case 3: bleCombo.write(KEY_MEDIA_VOLUME_UP); break;
        case 4: bleCombo.write(KEY_MEDIA_VOLUME_DOWN); break;
        case 5: bleCombo.write(KEY_MEDIA_MUTE); break;
        case 6: bleCombo.write(KEY_MEDIA_STOP); break;
    }
}

void triggerApplication(int selection) {
    if (!bleCombo.isConnected()) {
        showPopupMessage("Not Connected!", "", "");
        delay(500);
        displayMenu();
        return;
    }
    switch (selection) {
        case 0: runCommand("chrome"); break;
        case 1: runCommand("brave"); break;
        case 2: runCommand("vlc"); break;
        case 3: runCommand("calc"); break;
        case 4: runCommand("explorer"); break;
        case 5:
            bleCombo.press(KEY_LEFT_CTRL);
            bleCombo.press(KEY_LEFT_SHIFT);
            bleCombo.write(KEY_ESC);
            bleCombo.releaseAll();
            break;
    }
}

void runCommand(const char* command) {
    if (!bleCombo.isConnected()) return;
    
    bleCombo.press(KEY_LEFT_GUI);
    bleCombo.write('r');
    bleCombo.releaseAll();
    delay(400);
    bleCombo.print(command);
    delay(100);
    bleCombo.write(KEY_RETURN);
}

void triggerSystem(int selection) {
    if (!bleCombo.isConnected()) {
        showPopupMessage("Not Connected!", "", "");
        delay(500);
        displayMenu();
        return;
    }
    switch (selection) {
        case 0: // Lock
            bleCombo.press(KEY_LEFT_GUI);
            bleCombo.write('l');
            bleCombo.releaseAll();
            break;
        case 1: // Sleep
            runCommand("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
            break;
        case 2: // Restart
            runCommand("shutdown /r /t 0");
            break;
        case 3: // Shutdown
            runCommand("shutdown /s /t 0");
            break;
        case 4: // Desktop
            bleCombo.press(KEY_LEFT_GUI);
            bleCombo.write('d');
            bleCombo.releaseAll();
            break;
        case 5: // Vol+
            bleCombo.write(KEY_MEDIA_VOLUME_UP);
            break;
        case 6: // Vol-
            bleCombo.write(KEY_MEDIA_VOLUME_DOWN);
            break;
        case 7: // Mute
            bleCombo.write(KEY_MEDIA_MUTE);
            break;
    }
}

void triggerCamera(int selection) {
    if (!bleCombo.isConnected()) {
        showPopupMessage("Not Connected!", "", "");
        delay(500);
        displayMenu();
        return;
    }
    switch (selection) {
        case 0: runCommand("microsoft.windows.camera:"); break;
        case 1: runCommand("C:\\Program Files\\obs-studio\\bin\\64bit\\obs64.exe"); break;
        case 2: 
            bleCombo.press(KEY_LEFT_GUI);
            bleCombo.write(KEY_PRTSC);
            bleCombo.releaseAll();
            break;
        case 3: bleCombo.write(' '); break;
    }
}

void triggerUnlock() {
    if (!bleCombo.isConnected()) {
        showPopupMessage("Not Connected!", "", "");
        delay(500);
        displayMenu();
        return;
    }
    
    bleCombo.print(unlockPassword);
    bleCombo.write(KEY_RETURN);
    
    delay(500);
    showPopupMessage("Unlocked!", "Password sent", "");
    delay(500);
    currentMenu = MAIN_MENU;
    currentSelection = 0;
    menuTop = 0;
    displayMenu();
}

void triggerMouse(int selection) {
    if (!bleCombo.isConnected() && selection != 0) {
        showPopupMessage("Not Connected!", "", "");
        delay(500);
        displayMenu();
        return;
    }
    
    switch (selection) {
        case 0:
            enterAirMouse();
            break;
        case 1:
            if (bleCombo.isConnected()) bleCombo.move(30, 0);
            break;
        case 2:
            if (bleCombo.isConnected()) bleCombo.move(-30, 0);
            break;
        case 3:
            if (bleCombo.isConnected()) bleCombo.move(0, -30);
            break;
        case 4:
            if (bleCombo.isConnected()) bleCombo.move(0, 30);
            break;
        case 5:
            if (bleCombo.isConnected()) bleCombo.click(MOUSE_LEFT);
            break;
        case 6:
            if (bleCombo.isConnected()) bleCombo.click(MOUSE_RIGHT);
            break;
        case 7:
            if (bleCombo.isConnected()) bleCombo.click(MOUSE_MIDDLE);
            break;
        case 8:
            if (bleCombo.isConnected()) bleCombo.move(0, 0, 3);
            break;
        case 9:
            if (bleCombo.isConnected()) bleCombo.move(0, 0, -3);
            break;
        case 10:
            dragMode = !dragMode;
            displayMenu();
            break;
    }
}

// ============================================================
// DISPLAY FUNCTIONS - With Status at Top Left
// ============================================================

void displayMenu() {
    const char** items = nullptr;
    int length = 0;
    const char* title = "";
    int iconIndex = 4;

    switch (currentMenu) {
        case MAIN_MENU:
            items = mainMenuItems;
            length = mainMenuLength;
            title = "BLE Remote";
            break;
        case MOUSE_MENU:
            items = mouseItems;
            length = mouseLength;
            title = "Mouse";
            break;
        case KEYBOARD_MENU:
            items = keyboardItems;
            length = keyboardLength;
            title = "Keyboard";
            break;
        case MEDIA_MENU:
            items = mediaItems;
            length = mediaLength;
            title = "Media";
            break;
        case APP_MENU:
            items = appItems;
            length = appLength;
            title = "Apps";
            break;
        case CAMERA_MENU:
            items = cameraItems;
            length = cameraLength;
            title = "Camera";
            break;
        case SYSTEM_MENU:
            items = systemItems;
            length = systemLength;
            title = "System";
            break;
        case SETTINGS_MENU:
            items = bleSettingsItems;
            length = bleSettingsLength;
            title = "Settings";
            break;
        case SENSITIVITY_MENU:
            items = sensitivityItems;
            length = sensitivityLength;
            title = "Sensitivity";
            break;
        case UNLOCK_MENU:
            displayUnlockMenu();
            return;
        default:
            return;
    }

    u8g2.clearBuffer();
    
    int offsetY = 6;
    
    // STATUS - Top Left Corner
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setCursor(4, offsetY + 9);
    u8g2.print(bleCombo.isConnected() ? "●" : "○");
    
    // Show mode indicator
    if (currentMode == MODE_AIR_MOUSE) u8g2.print("AM");
    else if (currentMode == MODE_KEYBOARD) u8g2.print("KB");
    
    // TITLE - Centered
    u8g2.setFont(u8g2_font_6x13_tr);
    int titleWidth = u8g2.getStrWidth(title);
    int titleX = (128 - titleWidth) / 2;
    u8g2.setCursor(titleX, offsetY + 14);
    u8g2.print(title);
    
    // ROUNDED BOX
    int boxX = 4;
    int boxY = offsetY + 16;
    int boxW = 120;
    int boxH = 40;
    int cornerRadius = 10;
    
    u8g2.drawRFrame(boxX, boxY, boxW, boxH, cornerRadius);
    
    if (length <= 0) {
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(boxX + 16, boxY + 24);
        u8g2.print("No items");
        u8g2.sendBuffer();
        return;
    }
    
    // Show items inside box
    int visibleItems = 4;
    updateMenuScroll();
    
    int startItem = menuTop;
    int endItem = min(startItem + visibleItems, length);
    
    int itemHeight = 10;
    int totalItems = endItem - startItem;
    int totalHeight = totalItems * itemHeight;
    int startY = boxY + (boxH - totalHeight) / 2 + 7;
    
    for (int i = startItem; i < endItem; i++) {
        int posInList = i - startItem;
        int yPos = startY + posInList * itemHeight;
        int textWidth = u8g2.getStrWidth(items[i]);
        int textX = boxX + (boxW - textWidth) / 2;
        
        if (i == currentSelection) {
            int itemBoxX = boxX + 4;
            int itemBoxW = boxW - 8;
            int itemBoxY = yPos - 8;
            int itemBoxH = 10;
            
            u8g2.setDrawColor(0);
            u8g2.drawBox(itemBoxX, itemBoxY, itemBoxW, itemBoxH);
            u8g2.setDrawColor(1);
            
            u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.setCursor(textX, yPos);
            u8g2.print(items[i]);
        } else {
            u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.setCursor(textX, yPos);
            u8g2.print(items[i]);
        }
    }
    
    // Scroll indicators
    if (menuTop > 0) {
        u8g2.setCursor(boxX + 54, boxY + 5);
        u8g2.print("▲");
    }
    if (menuTop + visibleItems < length) {
        u8g2.setCursor(boxX + 54, boxY + boxH - 4);
        u8g2.print("▼");
    }
    
    u8g2.sendBuffer();
}

void displayAirMouse() {
    u8g2.clearBuffer();
    
    int offsetY = 6;
    
    // STATUS - Top Left Corner
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setCursor(4, offsetY + 9);
    u8g2.print(bleCombo.isConnected() ? "●" : "○");
    u8g2.print("AM");
    
    // TITLE
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.setCursor(24, offsetY + 14);
    u8g2.print("Air Mouse");
    
    // ROUNDED BOX
    int boxX = 4;
    int boxY = offsetY + 16;
    int boxW = 120;
    int boxH = 40;
    int cornerRadius = 10;
    
    u8g2.drawRFrame(boxX, boxY, boxW, boxH, cornerRadius);
    
    // Info inside box
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(boxX + 16, boxY + 17);
    u8g2.print("Speed: ");
    u8g2.print(sensitivityItems[sensitivity]);
    
    u8g2.setCursor(boxX + 16, boxY + 31);
    if (dragMode) {
        u8g2.print("DRAG MODE ON");
    } else {
        u8g2.print("DRAG MODE OFF");
    }
    
    u8g2.sendBuffer();
}

void displayKeyboardMode() {
    u8g2.clearBuffer();
    
    int offsetY = 6;
    
    // STATUS - Top Left Corner
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setCursor(4, offsetY + 9);
    u8g2.print(bleCombo.isConnected() ? "●" : "○");
    u8g2.print("KB");
    
    // TITLE
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.setCursor(24, offsetY + 14);
    u8g2.print("Keyboard");
    
    // ROUNDED BOX
    int boxX = 4;
    int boxY = offsetY + 16;
    int boxW = 120;
    int boxH = 40;
    int cornerRadius = 10;
    
    u8g2.drawRFrame(boxX, boxY, boxW, boxH, cornerRadius);
    
    // Info inside box
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(boxX + 16, boxY + 17);
    u8g2.print("↑ ↓ = Arrows");
    u8g2.setCursor(boxX + 16, boxY + 31);
    u8g2.print("SEL=Enter  BACK=Exit");
    
    u8g2.sendBuffer();
}

void displayUnlockMenu() {
    u8g2.clearBuffer();
    
    int offsetY = 6;
    
    // STATUS - Top Left Corner
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setCursor(4, offsetY + 9);
    u8g2.print(bleCombo.isConnected() ? "●" : "○");
    
    // TITLE
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.setCursor(32, offsetY + 14);
    u8g2.print("Unlock");
    
    // ROUNDED BOX
    int boxX = 4;
    int boxY = offsetY + 16;
    int boxW = 120;
    int boxH = 40;
    int cornerRadius = 10;
    
    u8g2.drawRFrame(boxX, boxY, boxW, boxH, cornerRadius);
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(boxX + 28, boxY + 24);
    u8g2.print("Press SELECT");
    
    u8g2.sendBuffer();
}

void showMPUStatus() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(16, "MPU6050 STATUS");
    CenteredStr(32, "Not Available");
    u8g2.setCursor(0, 48);
    u8g2.print("Air Mouse: ");
    if (currentMode == MODE_AIR_MOUSE) u8g2.print("ON");
    else u8g2.print("OFF");
    CenteredStr(63, "BACK to exit");
    u8g2.sendBuffer();
    delay(1500);
    displayMenu();
}

void showBluetoothStatus() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(14, "BLUETOOTH STATUS");
    if (bleCombo.isConnected())
        CenteredStr(30, "CONNECTED");
    else
        CenteredStr(30, "WAITING...");
    CenteredStr(46, "ESP32 Remote");
    CenteredStr(62, "BACK to exit");
    u8g2.sendBuffer();
    delay(1500);
    displayMenu();
}

// ============================================================
// CLEANUP
// ============================================================

void bleRemoteCleanup() {
    Serial.println("BLE Remote cleanup...");
    unsigned long startTime = millis();

    if (currentMode == MODE_AIR_MOUSE || currentMode == MODE_KEYBOARD) {
        exitMode();
        delay(50);
    }

    // BleCombo doesn't have a deinit, but we can stop it
    bleCombo.end();
    
    currentMode = MODE_MENU;
    currentMenu = MAIN_MENU;
    currentSelection = 0;
    menuTop = 0;
    movingUp = false;
    movingDown = false;
    movingLeft = false;
    movingRight = false;
    dragMode = false;
    clearPopup();

    for (int i = 0; i < 4; i++) {
        lastButtonState[i] = HIGH;
        buttonState[i] = HIGH;
        buttonHeld[i] = false;
        buttonPressStart[i] = 0;
        lastDebounceTime[i] = 0;
    }

    Serial.printf("BLE Remote cleanup complete (%lu ms)\n", millis() - startTime);
}