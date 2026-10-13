#ifndef CONFIG_H
#define CONFIG_H

#define EEPROM_SIZE 512
#define MAX_CODES 100
#define IR_FILE "/ir_codes.json"
#define CREDENTIALS_FILE "/credentials.json"

#define MAX_CREDENTIALS 50
#define DNS_PORT 53
#define MAX_MESSAGES 50
#define MAX_USERNAME 16
#define MAX_MESSAGE 300
#define MAX_CLIENTS 10
#define WS_PORT 81

#define ICONS_PER_ROW 3
#define ICONS_PER_COL 2
#define MAX_MENU_ITEMS 25
#define MAX_ITEM_LENGTH 20
#define DEBOUNCE 200
#define BLE_CHECK_INTERVAL 3000

#define DEAUTHER_AP_SSID "ESP32-Deauther"
#define DEAUTHER_AP_PASS "esp32wroom32"
#define DEAUTHER_NUM_FRAMES 16
#define DEAUTHER_CHANNEL_MAX 13
#define DEAUTHER_MAX_NETWORKS 20
#define DEAUTHER_WEB_PORT 82

#define MAX_PACKETS 200
#define MAX_NETWORK_DEVICES 50

#define EEPROM_WIFI_MAGIC 0x55AA
#define EEPROM_CREDS_START 200

#define BLE_ATTACK_INTERVAL 200

#endif