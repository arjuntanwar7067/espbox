#include "about.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "config.h"
#include <U8g2lib.h>
#include "pins.h"
#include <WiFi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define ABOUT_PAGES 3
int aboutPage = 0;
unsigned long aboutLastChange = 0;

const char* aboutLines[][6] = {
  // Page 0 - Main Info
  {
    "ESP32 Multi-Tool",
    "Version 3.0",
    "Built: " __DATE__,
    "Time: " __TIME__,
    "All Modules",
    "SELECT to change"
  },
  // Page 1 - Module List
  {
    "--- MODULES ---",
    "Chat | Rogue AP",
    "WiFi Scan | IR",
    "MPU6050 | Games",
    "BLE Remote | Creds",
    "Settings | About"
  },
  // Page 2 - System Info
  {
    "--- SYSTEM ---",
    "Flash: 4MB",
    "Free Heap:",
    "",
    "",
    "BACK to exit"
  }
};

void aboutSetup() {
    aboutPage = 0;
    aboutLastChange = millis();
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 2, 16, 16, icon_about);
    
    for(int i = 0; i < 6; i++) {
        int y = 20 + i * 7;
        CenteredStr(y, aboutLines[0][i]);
    }
    u8g2.sendBuffer();
}

void drawAboutPage(int page) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 2, 16, 16, icon_about);
    
    if(page == 0) {
        for(int i = 0; i < 6; i++) {
            int y = 20 + i * 7;
            CenteredStr(y, aboutLines[0][i]);
        }
    } else if(page == 1) {
        for(int i = 0; i < 6; i++) {
            int y = 16 + i * 8;
            CenteredStr(y, aboutLines[1][i]);
        }
    } else if(page == 2) {
        char heapStr[20];
        sprintf(heapStr, "Heap: %d KB", ESP.getFreeHeap() / 1024);
        
        u8g2.setCursor(10, 20);
        u8g2.print("--- SYSTEM ---");
        u8g2.setCursor(10, 32);
        u8g2.print("Flash: 4MB");
        u8g2.setCursor(10, 44);
        u8g2.print(heapStr);
        
        char chipStr[20];
        sprintf(chipStr, "Chip: %s", ESP.getChipModel());
        u8g2.setCursor(10, 56);
        u8g2.print(chipStr);
    }
    
    char pageStr[8];
    sprintf(pageStr, "%d/%d", page + 1, ABOUT_PAGES);
    u8g2.setCursor(0, 63);
    u8g2.print(pageStr);
    u8g2.drawStr(100, 63, "SEL:Next");
    
    u8g2.sendBuffer();
}

void aboutLoop() { 
    if(isButtonPressed(BTN_BACK)) { 
        exitModule(); 
        return; 
    }
    
    if(isButtonPressed(BTN_SELECT)) {
        aboutPage = (aboutPage + 1) % ABOUT_PAGES;
        drawAboutPage(aboutPage);
        delay(200);
    }
    
    delay(20); 
}