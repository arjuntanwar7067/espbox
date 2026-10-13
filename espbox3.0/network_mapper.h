#ifndef NETWORK_MAPPER_H
#define NETWORK_MAPPER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <lwip/netdb.h>

#define DEVICE_UNKNOWN 0
#define DEVICE_ROUTER 1
#define DEVICE_COMPUTER 2
#define DEVICE_MOBILE 3
#define DEVICE_IOT 4
#define DEVICE_PRINTER 5
#define DEVICE_CAMERA 6
#define DEVICE_TV 7
#define MAX_OPEN_PORTS 32
#define MAX_DEVICES 30
#define MAX_SCAN_NETWORKS 20
#define MAX_PASSWORD_LEN 64


// network_mapper.h - Add these constants at the top

#define KEYBOARD_ROWS 4
#define KEYBOARD_COLS 10

// Keyboard layouts (declare these as global or static)
extern const char* keyboardLetters[];
extern const char* keyboardNumbers[];
extern const char* keyboardSymbols[];
extern const int LETTER_ROWS;
extern const int NUMBER_ROWS;
extern const int SYMBOL_ROWS;
// ✅ ADD THIS: Keyboard constants


struct NetworkDevice {
    char ip[16];
    char mac[18];
    char hostname[32];
    char vendor[24];
    uint8_t deviceType;
    uint16_t openPorts[MAX_OPEN_PORTS];
    uint8_t openPortCount;
    uint16_t firstPort;
    uint16_t lastPort;
    char os[32];
    bool isVulnerable;
    char vulnerability[64];
    uint16_t responseTime;
    bool isOnline;
    uint8_t signalStrength;
};

struct NetworkInfo {
    char ssid[33];
    char bssid[18];
    int8_t rssi;
    uint8_t channel;
    uint8_t encryption;
    NetworkDevice devices[MAX_DEVICES];
    uint16_t deviceCount;
    uint16_t maxDevices;
    char gateway[16];
    char subnet[16];
    uint32_t scanTime;
};

enum NetMenuState {
    NET_MENU_MAIN,
    NET_MENU_SCAN_WIFI,
    NET_MENU_SELECT_NETWORK,
    NET_MENU_PASSWORD_ENTRY,
    NET_MENU_DEVICES,
    NET_MENU_DEVICE_INFO,
    NET_MENU_MANUAL_IP,
    NET_MENU_PORT_SCAN,
    NET_MENU_EXPORT
};

struct NetUIState {
    NetMenuState menuState;
    int selectedIndex;
    int scrollOffset;
    char manualIP[16];
    int manualIPPos;
    bool editingIP;
    int scanStart;
    int scanEnd;
    bool scanning;
    bool showProgress;
    uint32_t lastUpdate;
    int wifiNetworkIndex;
    bool inNetworkSelect;
    char password[MAX_PASSWORD_LEN];
    int passwordPos;
    bool editingPassword;
    bool shiftMode;
    bool symbolMode;
    int keyboardPage;
};

class NetworkMapper {
  private:
    NetworkInfo currentNetwork;
    uint16_t maxDevices;
    bool scanActive;
    bool scanComplete;
    IPAddress gatewayIP;
    IPAddress subnetMask;
    uint32_t scanStartTime;
    uint32_t scanProgress;
    
    char manualIP[16];
    int manualIPIndex;
    bool manualIPMode;
    
    int startIP;
    int endIP;
    bool usePingScan;
    bool usePortScan;
    bool useARP;
    
    struct WiFiNetwork {
        char ssid[33];
        char bssid[18];
        int channel;
        int rssi;
        uint8_t encryption;
    };
    WiFiNetwork wifiNetworks[MAX_SCAN_NETWORKS];
    int wifiNetworkCount;
    int selectedNetworkIndex;
    
    // Private methods
    void discoverDevices();
    void fingerprintDevices();
    void detectOS(NetworkDevice* device);
    void checkVulnerabilities(NetworkDevice* device);
    bool pingHost(IPAddress ip);
    bool pingHost(const char* ip);
    uint16_t getPortStatus(const char* ip, uint16_t port);
    const char* getVendorFromMAC(const char* mac);
    IPAddress getGatewayIP();
    IPAddress getSubnetMask();
    void getHostname(NetworkDevice* device);
    void getDeviceType(NetworkDevice* device);
    bool getMACFromIP(const char* ip, char* mac);
    void scanIPRange(uint8_t firstOctet, uint8_t secondOctet, uint8_t thirdOctet, int start, int end);
    void analyzeSelectedNetwork();
    
    // ✅ ADD THIS: selectNetwork declaration (private)
    void selectNetwork(int index);
    
  public:
    NetworkMapper(uint16_t max = MAX_DEVICES);
    ~NetworkMapper();
    
    bool startScan();
    bool startScanRange(int start, int end);
    bool scanSpecificIP(const char* ip);
    void stopScan();
    NetworkInfo* getNetworkInfo();
    NetworkDevice* getDevice(uint16_t index);
    uint16_t getDeviceCount();
    bool isScanning();
    bool isComplete();
    float getProgress();
    void reset();
    void exportToJSON();
    String getDeviceInfo(NetworkDevice* device);
    void setScanMode(bool ping, bool port, bool arp);
    
    void scanWiFiNetworks();
    int getWiFiNetworkCount();
    String getWiFiSSID(int index);
    String getWiFiBSSID(int index);
    int getWiFiChannel(int index);
    int getWiFiRSSI(int index);
    uint8_t getWiFiEncryption(int index);
    void selectWiFiNetwork(int index);
    bool connectToNetwork(int index, const char* password);
    
    void scanPorts(NetworkDevice* device);
};

void networkMapperSetup();
void networkMapperLoop();
void networkMapperCleanup();

#endif