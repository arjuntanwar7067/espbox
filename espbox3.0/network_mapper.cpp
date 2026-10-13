#include "network_mapper.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"
#include <U8g2lib.h>
#include <lwip/netdb.h>
#include <lwip/etharp.h>
#include <lwip/ip4_addr.h>
#include <esp_wifi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// =================================================
// KEYBOARD LAYOUTS
// =================================================

// Row 1: Letters (Lowercase by default)
const char* keyboardLettersLower[] = {
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm"
};

// Row 1: Letters (Uppercase when shift is ON)
const char* keyboardLettersUpper[] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
};
const int LETTER_ROWS = 3;

// Row 2: Numbers and basic symbols
const char* keyboardNumbers[] = {
    "1234567890",
    "!@#$%^&*()",
    "-_=+[]{}|"
};
const int NUMBER_ROWS = 3;

// Row 3: Special characters
const char* keyboardSymbols[] = {
    ";:'\",.<>/?",
    "~`\\|",
    "€£¥©®™"
};
const int SYMBOL_ROWS = 3;

// =================================================
// OUI Database - MAC vendor lookup
// =================================================

struct OUIVendor {
  const char* oui;
  const char* vendor;
};

const OUIVendor ouiDatabase[] = {
  {"00:00:0C", "Cisco"},
  {"00:01:5C", "Intel"},
  {"00:02:55", "HP"},
  {"00:03:73", "Samsung"},
  {"00:04:96", "LG"},
  {"00:05:5D", "Apple"},
  {"00:06:25", "Dell"},
  {"00:07:9B", "IBM"},
  {"00:08:74", "Sony"},
  {"00:09:2A", "Nokia"},
  {"00:0A:95", "Motorola"},
  {"00:0B:6B", "Microsoft"},
  {"00:0C:29", "VMware"},
  {"00:0D:37", "ASUS"},
  {"00:0E:58", "Acer"},
  {"00:0F:FE", "Realtek"},
  {"00:10:18", "Broadcom"},
  {"00:11:22", "Google"},
  {"00:12:7A", "Amazon"},
  {"00:13:72", "Huawei"},
  {"00:14:22", "Xiaomi"},
  {"00:15:5D", "Microsoft"},
  {"00:16:FE", "D-Link"},
  {"00:17:5A", "Dell"},
  {"00:18:4D", "Linksys"},
  {"00:19:99", "Samsung"},
  {"00:1A:2B", "LG"},
  {"00:1B:21", "Apple"},
  {"00:1C:25", "HP"},
  {"00:1D:8C", "TP-Link"},
  {"00:1E:52", "Cisco"},
  {"00:1F:33", "Netgear"},
  {"00:20:91", "Intel"},
  {"00:21:5E", "Motorola"},
  {"00:22:68", "Samsung"},
  {"00:23:12", "Microsoft"},
  {"00:24:46", "ASUS"},
  {"00:25:64", "Acer"},
  {"00:26:82", "Dell"},
  {"00:27:0E", "Huawei"},
  {"00:28:9B", "Xiaomi"},
  {"00:29:5C", "OnePlus"},
  {"00:30:65", "Sony"},
  {"00:50:56", "VMware"},
  {"00:60:97", "Linksys"},
  {"00:80:64", "Intel"},
  {"00:A0:C9", "IBM"},
  {"00:C0:49", "Realtek"},
  {"00:D0:59", "Apple"},
  {"00:E0:4C", "Dell"},
  {"70:1A:B8", "TP-Link"},
  {"70:4D:7B", "Netgear"},
  {"B0:5A:DA", "Apple"},
  {"B4:8B:19", "Dell"},
  {"B4:E6:2D", "Huawei"},
  {"B8:27:EB", "Raspberry Pi"},
  {"B8:CA:3A", "Samsung"},
  {"BC:A8:30", "Xiaomi"},
  {"C0:56:27", "Netgear"},
  {"C0:B5:D7", "LG"},
  {"CC:20:E8", "ASUS"},
  {"CC:3D:82", "Sony"},
  {"D0:9E:DF", "Acer"},
  {"D0:AD:08", "Google"},
  {"D4:61:9D", "OnePlus"},
  {"D4:CA:6D", "Xiaomi"},
  {"D8:50:E6", "Dell"},
  {"DC:A6:32", "Samsung"},
  {"E0:55:3D", "ASUS"},
  {"E4:5F:01", "Huawei"},
  {"E8:6A:64", "Intel"},
  {"EC:9B:F3", "Xiaomi"},
  {"F0:18:98", "Amazon"},
  {"F4:5C:89", "LG"},
  {"F8:4B:2B", "Apple"},
  {"FC:A8:9A", "Dell"},
  {"00:00:00", "Unknown"}
};

// =================================================
// FORWARD DECLARATIONS
// =================================================

static NetworkMapper* netMapper = nullptr;
static NetUIState ui;

static void showPopup(const char* title, const char* message);
static void handleMainMenu();
static void handleWiFiScan();
static void handleNetworkSelect();
static void handlePasswordEntry();
static void handleDeviceList();
static void handleDeviceInfo();
static void handleManualIP();
static void handlePortScan();
static void handleExport();

// =================================================
// HELPER FUNCTIONS
// =================================================

String getEncryptionTypeString(uint8_t encryptionType) {
  switch(encryptionType) {
    case WIFI_AUTH_OPEN: return "Open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3-PSK";
    default: return "Unknown";
  }
}

const char* NetworkMapper::getVendorFromMAC(const char* mac) {
  for (int i = 0; i < sizeof(ouiDatabase) / sizeof(OUIVendor); i++) {
    if (strncmp(mac, ouiDatabase[i].oui, 8) == 0) {
      return ouiDatabase[i].vendor;
    }
  }
  return "Unknown";
}

bool NetworkMapper::getMACFromIP(const char* ip, char* mac) {
  uint8_t ipParts[4];
  if (sscanf(ip, "%hhu.%hhu.%hhu.%hhu", &ipParts[0], &ipParts[1], &ipParts[2], &ipParts[3]) != 4) {
    strcpy(mac, "00:00:00:00:00:00");
    return false;
  }
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
          0xDE, 0xAD, 0xBE, ipParts[0], ipParts[1], ipParts[2]);
  return true;
}

// =================================================
// Ping Functions
// =================================================

bool NetworkMapper::pingHost(IPAddress ip) {
  WiFiClient client;
  if (client.connect(ip, 80, 100)) { client.stop(); return true; }
  if (client.connect(ip, 443, 100)) { client.stop(); return true; }
  if (client.connect(ip, 22, 100)) { client.stop(); return true; }
  if (client.connect(ip, 53, 100)) { client.stop(); return true; }
  return false;
}

bool NetworkMapper::pingHost(const char* ip) {
  IPAddress addr;
  addr.fromString(ip);
  return pingHost(addr);
}

// =================================================
// scanWiFiNetworks - PUBLIC
// =================================================

void NetworkMapper::scanWiFiNetworks() {
  Serial.println("📡 Scanning WiFi networks...");
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  CenteredStr(25, "Scanning WiFi...");
  CenteredStr(40, "Looking for networks");
  u8g2.sendBuffer();
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  int n = WiFi.scanNetworks();
  wifiNetworkCount = 0;
  
  if (n > 0) {
    for (int i = 0; i < n && i < MAX_SCAN_NETWORKS; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 0) {
        strncpy(wifiNetworks[wifiNetworkCount].ssid, ssid.c_str(), 32);
        wifiNetworks[wifiNetworkCount].ssid[32] = '\0';
        strncpy(wifiNetworks[wifiNetworkCount].bssid, WiFi.BSSIDstr(i).c_str(), 17);
        wifiNetworks[wifiNetworkCount].bssid[17] = '\0';
        wifiNetworks[wifiNetworkCount].channel = WiFi.channel(i);
        wifiNetworks[wifiNetworkCount].rssi = WiFi.RSSI(i);
        wifiNetworks[wifiNetworkCount].encryption = WiFi.encryptionType(i);
        wifiNetworkCount++;
      }
    }
    WiFi.scanDelete();
  }
  
  Serial.printf("✅ Found %d WiFi networks\n", wifiNetworkCount);
}

// =================================================
// scanPorts - PUBLIC
// =================================================

void NetworkMapper::scanPorts(NetworkDevice* device) {
  uint16_t commonPorts[] = {21, 22, 23, 25, 53, 80, 110, 123, 135, 137, 138, 
                            139, 143, 443, 445, 465, 587, 993, 995, 1433, 
                            1521, 1723, 3306, 3389, 5432, 5900, 6379, 8080, 8443, 27017};
  int portCount = sizeof(commonPorts) / sizeof(commonPorts[0]);
  
  device->firstPort = 0;
  device->lastPort = 0;
  device->openPortCount = 0;
  
  for (int i = 0; i < portCount; i++) {
    uint16_t port = commonPorts[i];
    uint16_t response = getPortStatus(device->ip, port);
    if (response > 0 && device->openPortCount < MAX_OPEN_PORTS) {
      device->openPorts[device->openPortCount] = port;
      device->openPortCount++;
      if (device->firstPort == 0 || port < device->firstPort) device->firstPort = port;
      if (port > device->lastPort) device->lastPort = port;
    }
  }
}

uint16_t NetworkMapper::getPortStatus(const char* ip, uint16_t port) {
  WiFiClient client;
  if (client.connect(ip, port, 50)) { client.stop(); return 1; }
  return 0;
}

// =================================================
// connectToNetwork with Password
// =================================================

bool NetworkMapper::connectToNetwork(int index, const char* password) {
  if (index < 0 || index >= wifiNetworkCount) return false;
  
  Serial.printf("🔗 Connecting to %s...\n", wifiNetworks[index].ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiNetworks[index].ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 100) {
    delay(100);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ Connected!");
    strcpy(currentNetwork.ssid, wifiNetworks[index].ssid);
    strcpy(currentNetwork.bssid, wifiNetworks[index].bssid);
    currentNetwork.rssi = wifiNetworks[index].rssi;
    currentNetwork.channel = wifiNetworks[index].channel;
    currentNetwork.encryption = wifiNetworks[index].encryption;
    analyzeSelectedNetwork();
    return true;
  }
  
  Serial.println("❌ Connection failed");
  return false;
}

// =================================================
// selectNetwork - PRIVATE
// =================================================

void NetworkMapper::selectNetwork(int index) {
  if (index < 0 || index >= wifiNetworkCount) return;
  selectedNetworkIndex = index;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiNetworks[index].ssid);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 50) {
    delay(100);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    strcpy(currentNetwork.ssid, wifiNetworks[index].ssid);
    strcpy(currentNetwork.bssid, wifiNetworks[index].bssid);
    currentNetwork.rssi = wifiNetworks[index].rssi;
    currentNetwork.channel = wifiNetworks[index].channel;
    currentNetwork.encryption = wifiNetworks[index].encryption;
    analyzeSelectedNetwork();
  } else {
    strcpy(currentNetwork.ssid, wifiNetworks[index].ssid);
    strcpy(currentNetwork.bssid, wifiNetworks[index].bssid);
    currentNetwork.rssi = wifiNetworks[index].rssi;
    currentNetwork.channel = wifiNetworks[index].channel;
    currentNetwork.encryption = wifiNetworks[index].encryption;
  }
}

// =================================================
// analyzeSelectedNetwork
// =================================================

void NetworkMapper::analyzeSelectedNetwork() {
  Serial.printf("🔍 Analyzing network: %s\n", currentNetwork.ssid);
  currentNetwork.deviceCount = 0;
  
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress gateway = getGatewayIP();
    IPAddress subnet = getSubnetMask();
    strcpy(currentNetwork.gateway, gateway.toString().c_str());
    strcpy(currentNetwork.subnet, subnet.toString().c_str());
    
    uint8_t firstOctet = gateway[0];
    uint8_t secondOctet = gateway[1];
    uint8_t thirdOctet = gateway[2];
    int startIP = (gateway[3] == 1) ? 2 : 1;
    int endIP = 254;
    
    scanIPRange(firstOctet, secondOctet, thirdOctet, startIP, endIP);
  } else {
    strcpy(currentNetwork.gateway, "Unknown");
    strcpy(currentNetwork.subnet, "Unknown");
    
    NetworkDevice* dev = &currentNetwork.devices[0];
    strcpy(dev->ip, "AP");
    strcpy(dev->mac, currentNetwork.bssid);
    const char* vendor = getVendorFromMAC(dev->mac);
    strcpy(dev->vendor, vendor);
    strcpy(dev->hostname, currentNetwork.ssid);
    dev->deviceType = DEVICE_ROUTER;
    dev->isOnline = true;
    currentNetwork.deviceCount = 1;
  }
}

// =================================================
// discoverDevices and scanIPRange
// =================================================

void NetworkMapper::discoverDevices() {
  Serial.println("🔍 Discovering devices...");
  currentNetwork.deviceCount = 0;
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Not connected to WiFi - limited scan");
    if (strlen(currentNetwork.bssid) > 0) {
      NetworkDevice* dev = &currentNetwork.devices[0];
      strcpy(dev->ip, "AP");
      strcpy(dev->mac, currentNetwork.bssid);
      const char* vendor = getVendorFromMAC(dev->mac);
      strcpy(dev->vendor, vendor);
      strcpy(dev->hostname, currentNetwork.ssid);
      dev->deviceType = DEVICE_ROUTER;
      dev->isOnline = true;
      currentNetwork.deviceCount = 1;
    }
    return;
  }
  
  IPAddress gateway = getGatewayIP();
  IPAddress subnet = getSubnetMask();
  strcpy(currentNetwork.gateway, gateway.toString().c_str());
  strcpy(currentNetwork.subnet, subnet.toString().c_str());
  
  uint8_t firstOctet = gateway[0];
  uint8_t secondOctet = gateway[1];
  uint8_t thirdOctet = gateway[2];
  int startIP = (gateway[3] == 1) ? 2 : 1;
  int endIP = 254;
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  CenteredStr(20, "Scanning Network...");
  CenteredStr(40, "Discovering devices");
  u8g2.sendBuffer();
  
  scanIPRange(firstOctet, secondOctet, thirdOctet, startIP, endIP);
  currentNetwork.scanTime = millis() - scanStartTime;
  Serial.printf("✅ Discovery complete: %d devices found\n", currentNetwork.deviceCount);
}

void NetworkMapper::scanIPRange(uint8_t firstOctet, uint8_t secondOctet, uint8_t thirdOctet, int start, int end) {
  char ipStr[16];
  int scanned = 0;
  int total = end - start + 1;
  
  for (int i = start; i <= end; i++) {
    sprintf(ipStr, "%d.%d.%d.%d", firstOctet, secondOctet, thirdOctet, i);
    
    if (scanned % 10 == 0) {
      char progressStr[32];
      int percent = (scanned * 100) / total;
      sprintf(progressStr, "Scanning %d%% (%d/%d)", percent, scanned, total);
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      CenteredStr(20, "Scanning Network...");
      CenteredStr(35, progressStr);
      u8g2.drawFrame(10, 48, 108, 8);
      u8g2.drawBox(12, 50, (percent * 104) / 100, 4);
      u8g2.sendBuffer();
    }
    scanned++;
    
    if (pingHost(ipStr)) {
      if (currentNetwork.deviceCount < MAX_DEVICES) {
        NetworkDevice* dev = &currentNetwork.devices[currentNetwork.deviceCount];
        strcpy(dev->ip, ipStr);
        
        char macStr[18];
        if (getMACFromIP(ipStr, macStr)) {
          strcpy(dev->mac, macStr);
        } else {
          uint8_t ipParts[4];
          sscanf(ipStr, "%hhu.%hhu.%hhu.%hhu", &ipParts[0], &ipParts[1], &ipParts[2], &ipParts[3]);
          sprintf(dev->mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
                  0xDE, 0xAD, 0xBE, ipParts[0], ipParts[1], ipParts[2]);
        }
        
        const char* vendor = getVendorFromMAC(dev->mac);
        strcpy(dev->vendor, vendor);
        getHostname(dev);
        getDeviceType(dev);
        dev->isOnline = true;
        currentNetwork.deviceCount++;
      }
    }
    delay(2);
  }
}

// =================================================
// REST OF PRIVATE FUNCTIONS
// =================================================

IPAddress NetworkMapper::getGatewayIP() {
  if (WiFi.gatewayIP() != IPAddress(0,0,0,0)) return WiFi.gatewayIP();
  return IPAddress(192,168,1,1);
}

IPAddress NetworkMapper::getSubnetMask() {
  if (WiFi.subnetMask() != IPAddress(0,0,0,0)) return WiFi.subnetMask();
  return IPAddress(255,255,255,0);
}

void NetworkMapper::getHostname(NetworkDevice* device) {
  struct hostent* host = gethostbyname(device->ip);
  if (host != nullptr && host->h_name != nullptr) {
    strncpy(device->hostname, host->h_name, 31);
    device->hostname[31] = '\0';
  } else {
    strcpy(device->hostname, "Unknown");
  }
}

void NetworkMapper::getDeviceType(NetworkDevice* device) {
  String vendor = String(device->vendor);
  device->deviceType = DEVICE_UNKNOWN;
  
  if (vendor.indexOf("Router") >= 0 || vendor.indexOf("Cisco") >= 0 || 
      vendor.indexOf("Netgear") >= 0 || vendor.indexOf("TP-Link") >= 0) {
    device->deviceType = DEVICE_ROUTER;
  } else if (vendor.indexOf("Apple") >= 0 || vendor.indexOf("Samsung") >= 0 || 
             vendor.indexOf("LG") >= 0 || vendor.indexOf("Sony") >= 0) {
    device->deviceType = DEVICE_MOBILE;
  } else if (vendor.indexOf("Dell") >= 0 || vendor.indexOf("HP") >= 0 || 
             vendor.indexOf("Intel") >= 0 || vendor.indexOf("Lenovo") >= 0) {
    device->deviceType = DEVICE_COMPUTER;
  } else if (vendor.indexOf("Raspberry") >= 0 || vendor.indexOf("Arduino") >= 0 ||
             vendor.indexOf("ESP") >= 0) {
    device->deviceType = DEVICE_IOT;
  } else if (vendor.indexOf("Print") >= 0 || vendor.indexOf("Brother") >= 0 ||
             vendor.indexOf("Canon") >= 0) {
    device->deviceType = DEVICE_PRINTER;
  } else if (vendor.indexOf("Camera") >= 0 || vendor.indexOf("Axis") >= 0) {
    device->deviceType = DEVICE_CAMERA;
  } else if (vendor.indexOf("TV") >= 0 || vendor.indexOf("TCL") >= 0) {
    device->deviceType = DEVICE_TV;
  }
}

void NetworkMapper::detectOS(NetworkDevice* device) {
  strcpy(device->os, "Unknown");
  bool hasPort22 = false, hasPort80 = false, hasPort443 = false;
  bool hasPort135 = false, hasPort139 = false, hasPort445 = false;
  bool hasPort1433 = false, hasPort3306 = false;
  
  for (int i = 0; i < device->openPortCount; i++) {
    uint16_t port = device->openPorts[i];
    if (port == 22) hasPort22 = true;
    else if (port == 80) hasPort80 = true;
    else if (port == 443) hasPort443 = true;
    else if (port == 135) hasPort135 = true;
    else if (port == 139) hasPort139 = true;
    else if (port == 445) hasPort445 = true;
    else if (port == 1433) hasPort1433 = true;
    else if (port == 3306) hasPort3306 = true;
  }
  
  if (hasPort135 && hasPort139 && hasPort445) strcpy(device->os, "Windows");
  else if (hasPort22 && hasPort80) strcpy(device->os, "Linux");
  else if (hasPort22 && !hasPort80 && !hasPort443) strcpy(device->os, "macOS");
  else if (hasPort80 && hasPort443) strcpy(device->os, "Web Server");
  else if (hasPort1433) strcpy(device->os, "MS SQL Server");
  else if (hasPort3306) strcpy(device->os, "MySQL Server");
  else if (hasPort22) strcpy(device->os, "SSH Server");
}

void NetworkMapper::checkVulnerabilities(NetworkDevice* device) {
  device->isVulnerable = false;
  strcpy(device->vulnerability, "None");
  
  bool hasPort22 = false, hasPort23 = false, hasPort21 = false;
  bool hasPort445 = false, hasPort3306 = false, hasPort1433 = false;
  
  for (int i = 0; i < device->openPortCount; i++) {
    uint16_t port = device->openPorts[i];
    if (port == 22) hasPort22 = true;
    else if (port == 23) hasPort23 = true;
    else if (port == 21) hasPort21 = true;
    else if (port == 445) hasPort445 = true;
    else if (port == 3306) hasPort3306 = true;
    else if (port == 1433) hasPort1433 = true;
  }
  
  if (hasPort22) { device->isVulnerable = true; strcpy(device->vulnerability, "SSH - Weak passwords"); }
  if (hasPort23) { device->isVulnerable = true; strcpy(device->vulnerability, "Telnet - Unencrypted"); }
  if (hasPort21) { device->isVulnerable = true; strcpy(device->vulnerability, "FTP - Anonymous access"); }
  if (hasPort445 && strcmp(device->os, "Windows") == 0) { device->isVulnerable = true; strcpy(device->vulnerability, "SMB - EternalBlue risk"); }
}

void NetworkMapper::fingerprintDevices() {
  for (uint16_t i = 0; i < currentNetwork.deviceCount; i++) {
    NetworkDevice* dev = &currentNetwork.devices[i];
    const char* vendor = getVendorFromMAC(dev->mac);
    strcpy(dev->vendor, vendor);
    getDeviceType(dev);
    if (strcmp(dev->hostname, "Unknown") == 0) getHostname(dev);
  }
}

void NetworkMapper::exportToJSON() {
  Serial.println("📤 Exporting network map to JSON...");
  Serial.println("{");
  Serial.println("  \"network\": {");
  Serial.printf("    \"ssid\": \"%s\",\n", currentNetwork.ssid);
  Serial.printf("    \"bssid\": \"%s\",\n", currentNetwork.bssid);
  Serial.printf("    \"rssi\": %d,\n", currentNetwork.rssi);
  Serial.printf("    \"channel\": %d,\n", currentNetwork.channel);
  Serial.printf("    \"encryption\": \"%s\",\n", getEncryptionTypeString(currentNetwork.encryption).c_str());
  Serial.printf("    \"gateway\": \"%s\",\n", currentNetwork.gateway);
  Serial.printf("    \"subnet\": \"%s\",\n", currentNetwork.subnet);
  Serial.printf("    \"devices\": %d\n", currentNetwork.deviceCount);
  Serial.println("  },");
  Serial.println("  \"devices\": [");
  
  for (uint16_t i = 0; i < currentNetwork.deviceCount; i++) {
    NetworkDevice* dev = &currentNetwork.devices[i];
    Serial.printf("    {\n");
    Serial.printf("      \"ip\": \"%s\",\n", dev->ip);
    Serial.printf("      \"mac\": \"%s\",\n", dev->mac);
    Serial.printf("      \"vendor\": \"%s\",\n", dev->vendor);
    Serial.printf("      \"hostname\": \"%s\",\n", dev->hostname);
    Serial.printf("      \"os\": \"%s\",\n", dev->os);
    Serial.printf("      \"deviceType\": %d,\n", dev->deviceType);
    Serial.printf("      \"vulnerable\": %s,\n", dev->isVulnerable ? "true" : "false");
    Serial.printf("      \"vulnerability\": \"%s\",\n", dev->vulnerability);
    Serial.printf("      \"openPorts\": [");
    for (int j = 0; j < dev->openPortCount; j++) {
      if (j > 0) Serial.print(",");
      Serial.printf("%d", dev->openPorts[j]);
    }
    Serial.println("]");
    if (i < currentNetwork.deviceCount - 1) Serial.println("    },");
    else Serial.println("    }");
  }
  Serial.println("  ]");
  Serial.println("}");
}

// =================================================
// PUBLIC METHODS
// =================================================

NetworkMapper::NetworkMapper(uint16_t max) {
  maxDevices = max;
  currentNetwork.deviceCount = 0;
  currentNetwork.maxDevices = max;
  scanActive = false;
  scanComplete = false;
  scanStartTime = 0;
  scanProgress = 0;
  gatewayIP = IPAddress(0,0,0,0);
  subnetMask = IPAddress(255,255,255,0);
  usePingScan = true;
  usePortScan = false;
  useARP = true;
  manualIPIndex = 0;
  manualIPMode = false;
  strcpy(manualIP, "192.168.1.1");
  wifiNetworkCount = 0;
  selectedNetworkIndex = 0;
}

NetworkMapper::~NetworkMapper() {}

void NetworkMapper::stopScan() { scanActive = false; }
NetworkInfo* NetworkMapper::getNetworkInfo() { return &currentNetwork; }
NetworkDevice* NetworkMapper::getDevice(uint16_t index) {
  if (index >= currentNetwork.deviceCount) return nullptr;
  return &currentNetwork.devices[index];
}
uint16_t NetworkMapper::getDeviceCount() { return currentNetwork.deviceCount; }
bool NetworkMapper::isScanning() { return scanActive; }
bool NetworkMapper::isComplete() { return scanComplete; }
float NetworkMapper::getProgress() {
  if (currentNetwork.deviceCount == 0) return 0;
  return (float)currentNetwork.deviceCount / maxDevices;
}
void NetworkMapper::reset() {
  currentNetwork.deviceCount = 0;
  scanComplete = false;
  scanActive = false;
}
void NetworkMapper::setScanMode(bool ping, bool port, bool arp) {
  usePingScan = ping;
  usePortScan = port;
  useARP = arp;
}
int NetworkMapper::getWiFiNetworkCount() { return wifiNetworkCount; }
String NetworkMapper::getWiFiSSID(int index) {
  if (index < 0 || index >= wifiNetworkCount) return "";
  return String(wifiNetworks[index].ssid);
}
String NetworkMapper::getWiFiBSSID(int index) {
  if (index < 0 || index >= wifiNetworkCount) return "";
  return String(wifiNetworks[index].bssid);
}
int NetworkMapper::getWiFiChannel(int index) {
  if (index < 0 || index >= wifiNetworkCount) return 0;
  return wifiNetworks[index].channel;
}
int NetworkMapper::getWiFiRSSI(int index) {
  if (index < 0 || index >= wifiNetworkCount) return 0;
  return wifiNetworks[index].rssi;
}
uint8_t NetworkMapper::getWiFiEncryption(int index) {
  if (index < 0 || index >= wifiNetworkCount) return WIFI_AUTH_OPEN;
  return wifiNetworks[index].encryption;
}
void NetworkMapper::selectWiFiNetwork(int index) {
  if (index < 0 || index >= wifiNetworkCount) return;
  selectNetwork(index);
}

bool NetworkMapper::startScan() {
  if (scanActive) return false;
  
  scanActive = true;
  scanComplete = false;
  scanStartTime = millis();
  currentNetwork.deviceCount = 0;
  scanProgress = 0;
  
  Serial.println("📡 Starting network scan...");
  
  if (WiFi.status() == WL_CONNECTED) {
    strcpy(currentNetwork.ssid, WiFi.SSID().c_str());
    strcpy(currentNetwork.bssid, WiFi.BSSIDstr().c_str());
    currentNetwork.rssi = WiFi.RSSI();
    currentNetwork.channel = WiFi.channel();
    currentNetwork.encryption = WiFi.encryptionType(0);
  } else {
    scanWiFiNetworks();
    if (wifiNetworkCount > 0) {
      int bestIndex = 0;
      int bestRSSI = -1000;
      for (int i = 0; i < wifiNetworkCount; i++) {
        if (wifiNetworks[i].rssi > bestRSSI) {
          bestRSSI = wifiNetworks[i].rssi;
          bestIndex = i;
        }
      }
      selectNetwork(bestIndex);
    } else {
      discoverDevices();
    }
  }
  
  discoverDevices();
  fingerprintDevices();
  
  if (usePortScan) {
    for (uint16_t i = 0; i < currentNetwork.deviceCount; i++) {
      scanPorts(&currentNetwork.devices[i]);
    }
    for (uint16_t i = 0; i < currentNetwork.deviceCount; i++) {
      detectOS(&currentNetwork.devices[i]);
    }
    for (uint16_t i = 0; i < currentNetwork.deviceCount; i++) {
      checkVulnerabilities(&currentNetwork.devices[i]);
    }
  }
  
  scanActive = false;
  scanComplete = true;
  
  Serial.printf("✅ Scan complete! Found %d devices\n", currentNetwork.deviceCount);
  return true;
}

bool NetworkMapper::scanSpecificIP(const char* ip) {
  if (scanActive) return false;
  
  currentNetwork.deviceCount = 0;
  
  if (pingHost(ip)) {
    NetworkDevice* dev = &currentNetwork.devices[0];
    strcpy(dev->ip, ip);
    
    char macStr[18];
    if (getMACFromIP(ip, macStr)) {
      strcpy(dev->mac, macStr);
    } else {
      uint8_t ipParts[4];
      sscanf(ip, "%hhu.%hhu.%hhu.%hhu", &ipParts[0], &ipParts[1], &ipParts[2], &ipParts[3]);
      sprintf(dev->mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
              0xDE, 0xAD, 0xBE, ipParts[0], ipParts[1], ipParts[2]);
    }
    
    const char* vendor = getVendorFromMAC(dev->mac);
    strcpy(dev->vendor, vendor);
    getHostname(dev);
    getDeviceType(dev);
    dev->isOnline = true;
    currentNetwork.deviceCount = 1;
    scanComplete = true;
    return true;
  }
  return false;
}

// =================================================
// POPUP FUNCTION
// =================================================

static void showPopup(const char* title, const char* message) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawFrame(10, 15, 108, 30);
  u8g2.drawBox(12, 17, 104, 26);
  u8g2.setDrawColor(0);
  CenteredStr(25, title);
  CenteredStr(37, message);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
  delay(1500);
}

// =================================================
// KEYBOARD DRAWING - 4 BUTTONS
// =================================================

static void drawKeyboard() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  
  // PASSWORD LINE AT TOP
  u8g2.setCursor(2, 10);
  u8g2.print("Pass: ");
  
  String pwd = String(ui.password);
  int pwdLen = pwd.length();
  
  int maxVisible = 14;
  int startChar = 0;
  if (ui.passwordPos >= maxVisible) startChar = ui.passwordPos - maxVisible + 1;
  if (startChar > pwdLen - maxVisible) startChar = pwdLen - maxVisible;
  if (startChar < 0) startChar = 0;
  
  int displayLen = min(pwdLen - startChar, maxVisible);
  for (int i = 0; i < displayLen; i++) {
    int idx = startChar + i;
    int x = 40 + i * 6;
    
    if (idx == ui.passwordPos && ui.editingPassword) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(x - 1, 2, 7, 10);
      u8g2.setDrawColor(1);
      u8g2.setCursor(x, 10);
      u8g2.print(pwd.charAt(idx));
    } else {
      u8g2.setCursor(x, 10);
      u8g2.print(pwd.charAt(idx));
    }
  }
  
  if (ui.editingPassword) {
    u8g2.drawFrame(38, 0, displayLen * 6 + 4, 12);
  }
  
  u8g2.setCursor(115, 10);
  char lenStr[16];
  if (pwdLen > 0) sprintf(lenStr, "%d/%d", ui.passwordPos + 1, pwdLen);
  else sprintf(lenStr, "0/0");
  u8g2.print(lenStr);
  
  if (startChar > 0) { u8g2.setCursor(38, 10); u8g2.print("<"); }
  if (startChar + maxVisible < pwdLen) { u8g2.setCursor(110, 10); u8g2.print(">"); }
  
  // KEYBOARD LAYOUT
  const char** currentLayout;
  int rows = LETTER_ROWS;
  
  if (ui.symbolMode) {
    currentLayout = keyboardSymbols;
    rows = SYMBOL_ROWS;
  } else if (ui.keyboardPage == 1) {
    currentLayout = keyboardNumbers;
    rows = NUMBER_ROWS;
  } else {
    if (ui.shiftMode) currentLayout = keyboardLettersUpper;
    else currentLayout = keyboardLettersLower;
    rows = LETTER_ROWS;
  }
  
  int keyW = 11;
  int keyH = 8;
  int startY = 16;
  int startX = 2;
  int maxKeyCols = 10;
  int scrollOffset = 0;
  
  if (ui.passwordPos >= maxKeyCols) {
    scrollOffset = ui.passwordPos - maxKeyCols + 1;
  }
  
  for (int row = 0; row < rows; row++) {
    int rowLen = strlen(currentLayout[row]);
    int displayStart = scrollOffset;
    int displayEnd = min(scrollOffset + maxKeyCols, rowLen);
    int displayCount = displayEnd - displayStart;
    
    int offset = (maxKeyCols - rowLen) * keyW / 2;
    if (rowLen < maxKeyCols) {
      displayStart = 0;
      displayCount = rowLen;
      offset = (maxKeyCols - rowLen) * keyW / 2;
    }
    
    for (int col = 0; col < displayCount; col++) {
      int actualCol = displayStart + col;
      int x = startX + col * keyW + offset;
      int y = startY + row * (keyH + 1);
      
      char c = currentLayout[row][actualCol];
      bool selected = (ui.selectedIndex == row && ui.passwordPos == actualCol);
      
      if (selected) {
        u8g2.setDrawColor(0);
        u8g2.drawBox(x, y, keyW, keyH);
        u8g2.setDrawColor(1);
        u8g2.setCursor(x + 2, y + keyH - 1);
        u8g2.print(c);
      } else {
        u8g2.drawFrame(x, y, keyW, keyH);
        u8g2.setCursor(x + 2, y + keyH - 1);
        u8g2.print(c);
      }
    }
  }
  
  if (scrollOffset > 0) { u8g2.setCursor(0, 22); u8g2.print("◄"); }
  if (scrollOffset + maxKeyCols < 10) { u8g2.setCursor(120, 22); u8g2.print("►"); }
  
  // MODE BUTTONS ROW
  int y = startY + rows * (keyH + 1);
  int modeCount = 4;
  int modeWidth = 22;
  int totalModeWidth = modeCount * modeWidth + (modeCount - 1) * 2;
  int modeStartX = (128 - totalModeWidth) / 2;
  
  const char* modeLabels[] = {"ABC", "123", "#@$", "⇧"};
  
  for (int i = 0; i < modeCount; i++) {
    int x = modeStartX + i * (modeWidth + 2);
    int idx = ui.selectedIndex - rows;
    bool selected = (idx == i);
    
    bool isActive = false;
    if (i == 0 && !ui.symbolMode && ui.keyboardPage == 0) isActive = true;
    if (i == 1 && !ui.symbolMode && ui.keyboardPage == 1) isActive = true;
    if (i == 2 && ui.symbolMode) isActive = true;
    if (i == 3 && ui.shiftMode) isActive = true;
    
    if (selected || isActive) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(x, y, modeWidth, keyH);
      u8g2.setDrawColor(1);
      u8g2.setCursor(x + 4, y + keyH - 1);
      u8g2.print(modeLabels[i]);
      if (isActive && !selected) u8g2.drawFrame(x, y, modeWidth, keyH);
    } else {
      u8g2.drawFrame(x, y, modeWidth, keyH);
      u8g2.setCursor(x + 4, y + keyH - 1);
      u8g2.print(modeLabels[i]);
    }
  }
  
  // BACKSPACE AND CONNECT BUTTONS
  int y2 = y + keyH + 1;
  int btnWidth = 40;
  int gap = 8;
  int totalWidth = btnWidth * 2 + gap;
  int startX2 = (128 - totalWidth) / 2;
  
  // Backspace button
  int x = startX2;
  int idx = ui.selectedIndex - rows;
  bool selected = (idx == modeCount);
  
  if (selected) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(x, y2, btnWidth, keyH);
    u8g2.setDrawColor(1);
    u8g2.setCursor(x + 6, y2 + keyH - 1);
    u8g2.print("⌫ Del");
  } else {
    u8g2.drawFrame(x, y2, btnWidth, keyH);
    u8g2.setCursor(x + 6, y2 + keyH - 1);
    u8g2.print("⌫ Del");
  }
  
  // Connect button
  x = startX2 + btnWidth + gap;
  selected = (idx == modeCount + 1);
  
  if (selected) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(x, y2, btnWidth, keyH);
    u8g2.setDrawColor(1);
    u8g2.setCursor(x + 8, y2 + keyH - 1);
    u8g2.print("✅ Go");
  } else {
    u8g2.drawFrame(x, y2, btnWidth, keyH);
    u8g2.setCursor(x + 8, y2 + keyH - 1);
    u8g2.print("✅ Go");
  }
  
  u8g2.sendBuffer();
}

// =================================================
// PASSWORD ENTRY HANDLER - 4 BUTTONS
// =================================================

static void handlePasswordEntry() {
  drawKeyboard();
  
  static unsigned long lastButtonTime = 0;
  unsigned long now = millis();
  
  if (now - lastButtonTime < 150) {
    delay(10);
    return;
  }
  
  const char** currentLayout;
  int rows = LETTER_ROWS;
  
  if (ui.symbolMode) {
    currentLayout = keyboardSymbols;
    rows = SYMBOL_ROWS;
  } else if (ui.keyboardPage == 1) {
    currentLayout = keyboardNumbers;
    rows = NUMBER_ROWS;
  } else {
    if (ui.shiftMode) currentLayout = keyboardLettersUpper;
    else currentLayout = keyboardLettersLower;
    rows = LETTER_ROWS;
  }
  
  int maxIndex = rows + 4 + 2 - 1;
  
  // UP/DOWN navigation
  if (isButtonPressed(BTN_UP)) {
    lastButtonTime = now;
    if (ui.selectedIndex > 0) {
      ui.selectedIndex--;
      int rowLen = 0;
      if (ui.selectedIndex < rows) rowLen = strlen(currentLayout[ui.selectedIndex]);
      if (ui.passwordPos >= rowLen && rowLen > 0) ui.passwordPos = rowLen - 1;
      if (rowLen == 0) ui.passwordPos = 0;
    } else {
      ui.selectedIndex = maxIndex;
    }
    return;
  }
  
  if (isButtonPressed(BTN_DOWN)) {
    lastButtonTime = now;
    if (ui.selectedIndex < maxIndex) {
      ui.selectedIndex++;
      int rowLen = 0;
      if (ui.selectedIndex < rows) rowLen = strlen(currentLayout[ui.selectedIndex]);
      if (ui.passwordPos >= rowLen && rowLen > 0) ui.passwordPos = rowLen - 1;
      if (rowLen == 0) ui.passwordPos = 0;
    } else {
      ui.selectedIndex = 0;
    }
    return;
  }
  
  // LEFT: SELECT + UP
  if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_UP)) {
    lastButtonTime = now;
    if (ui.selectedIndex < rows && ui.passwordPos > 0) ui.passwordPos--;
    return;
  }
  
  // RIGHT: SELECT + DOWN
  if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_DOWN)) {
    lastButtonTime = now;
    if (ui.selectedIndex < rows) {
      int maxCol = strlen(currentLayout[ui.selectedIndex]) - 1;
      if (ui.passwordPos < maxCol) ui.passwordPos++;
    }
    return;
  }
  
  // SELECT (single press)
  if (isButtonPressed(BTN_SELECT) && !isButtonPressed(BTN_UP) && !isButtonPressed(BTN_DOWN)) {
    lastButtonTime = now;
    
    // Mode buttons
    if (ui.selectedIndex >= rows && ui.selectedIndex < rows + 4) {
      int modeIdx = ui.selectedIndex - rows;
      switch (modeIdx) {
        case 0: ui.keyboardPage = 0; ui.symbolMode = false; ui.shiftMode = false; ui.selectedIndex = 0; ui.passwordPos = 0; break;
        case 1: ui.keyboardPage = 1; ui.symbolMode = false; ui.shiftMode = false; ui.selectedIndex = 0; ui.passwordPos = 0; break;
        case 2: ui.symbolMode = true; ui.keyboardPage = 0; ui.shiftMode = false; ui.selectedIndex = 0; ui.passwordPos = 0; break;
        case 3: ui.shiftMode = !ui.shiftMode; break;
      }
      return;
    }
    
    // Backspace
    if (ui.selectedIndex == rows + 4) {
      int len = strlen(ui.password);
      if (len > 0) { ui.password[len - 1] = '\0'; if (ui.passwordPos > 0) ui.passwordPos--; }
      return;
    }
    
    // Connect
    if (ui.selectedIndex == rows + 5) {
      if (strlen(ui.password) == 0) { showPopup("Error", "Password required!"); return; }
      
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      CenteredStr(25, "Connecting...");
      char ssid[40];
      sprintf(ssid, "SSID: %s", netMapper->getWiFiSSID(ui.wifiNetworkIndex).c_str());
      CenteredStr(40, ssid);
      u8g2.sendBuffer();
      
      bool connected = netMapper->connectToNetwork(ui.wifiNetworkIndex, ui.password);
      
      if (connected) {
        showPopup("✅ Connected!", "WiFi connected successfully!");
        ui.menuState = NET_MENU_MAIN;
        ui.selectedIndex = 0;
        ui.password[0] = '\0';
        ui.passwordPos = 0;
        ui.shiftMode = false;
      } else {
        showPopup("❌ Failed", "Check password and try again");
        ui.password[0] = '\0';
        ui.passwordPos = 0;
        ui.shiftMode = false;
      }
      return;
    }
    
    // Type character
    if (ui.selectedIndex < rows) {
      int rowLen = strlen(currentLayout[ui.selectedIndex]);
      if (ui.passwordPos < rowLen) {
        char c = currentLayout[ui.selectedIndex][ui.passwordPos];
        if (ui.shiftMode && !ui.symbolMode && ui.keyboardPage == 0) ui.shiftMode = false;
        int len = strlen(ui.password);
        if (len < MAX_PASSWORD_LEN - 1) {
          ui.password[len] = c;
          ui.password[len + 1] = '\0';
          ui.passwordPos++;
        }
      }
    }
    return;
  }
  
  // BACK
  if (isButtonPressed(BTN_BACK)) {
    lastButtonTime = now;
    ui.password[0] = '\0';
    ui.passwordPos = 0;
    ui.shiftMode = false;
    ui.menuState = NET_MENU_SELECT_NETWORK;
    return;
  }
}

// =================================================
// MAIN MENU
// =================================================

static void handleMainMenu() {
  const char* items[] = {
    "📡 Scan Networks",
    "📶 Select Network",
    "📱 View Devices",
    "🔍 Manual IP Scan",
    "🔓 Port Scan",
    "📤 Export JSON",
    "⬅️ Back"
  };
  int count = 7;
  
  drawScrollingMenu(items, count, ui.selectedIndex, 6, "Network Mapper", 12);
  
  if (isButtonPressed(BTN_UP)) ui.selectedIndex = (ui.selectedIndex - 1 + count) % count;
  if (isButtonPressed(BTN_DOWN)) ui.selectedIndex = (ui.selectedIndex + 1) % count;
  if (isButtonPressed(BTN_SELECT)) {
    switch (ui.selectedIndex) {
      case 0:
        ui.menuState = NET_MENU_SCAN_WIFI;
        ui.scanning = true;
        netMapper->scanWiFiNetworks();
        ui.scanning = false;
        netMapper->startScan();
        ui.menuState = NET_MENU_DEVICES;
        ui.selectedIndex = 0;
        showPopup("Scan Complete", String(netMapper->getDeviceCount() + " devices found").c_str());
        break;
      case 1:
        ui.menuState = NET_MENU_SELECT_NETWORK;
        ui.wifiNetworkIndex = 0;
        break;
      case 2:
        if (netMapper->getDeviceCount() > 0) {
          ui.menuState = NET_MENU_DEVICES;
          ui.selectedIndex = 0;
        } else {
          showPopup("No devices found", "Press SELECT to scan!");
        }
        break;
      case 3:
        ui.menuState = NET_MENU_MANUAL_IP;
        ui.editingIP = true;
        ui.manualIPPos = 0;
        strcpy(ui.manualIP, "192.168.1.1");
        break;
      case 4:
        ui.menuState = NET_MENU_PORT_SCAN;
        break;
      case 5:
        ui.menuState = NET_MENU_EXPORT;
        break;
      case 6:
        exitModule();
        return;
    }
  }
}

// =================================================
// WIFI SCAN
// =================================================

static void handleWiFiScan() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBMP(56, 2, 16, 16, icon_wifi);
  CenteredStr(15, "WiFi Scan");
  u8g2.drawLine(0, 18, 128, 18);
  
  char buf[30];
  sprintf(buf, "Networks: %d", netMapper->getWiFiNetworkCount());
  CenteredStr(30, buf);
  
  if (ui.scanning) CenteredStr(45, "⏳ Scanning...");
  else CenteredStr(45, "SEL: Rescan");
  CenteredStr(60, "BACK: Menu");
  
  u8g2.sendBuffer();
  
  if (isButtonPressed(BTN_SELECT)) {
    ui.scanning = true;
    netMapper->scanWiFiNetworks();
    ui.scanning = false;
  }
  if (isButtonPressed(BTN_BACK)) ui.menuState = NET_MENU_MAIN;
}

// =================================================
// NETWORK SELECT
// =================================================

static void handleNetworkSelect() {
  int count = netMapper->getWiFiNetworkCount();
  
  if (count == 0) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(25, "No networks found");
    CenteredStr(40, "SEL: Scan  BACK: Menu");
    u8g2.sendBuffer();
    if (isButtonPressed(BTN_SELECT)) netMapper->scanWiFiNetworks();
    if (isButtonPressed(BTN_BACK)) ui.menuState = NET_MENU_MAIN;
    return;
  }
  
  String items[20];
  for (int i = 0; i < count && i < 20; i++) {
    String ssid = netMapper->getWiFiSSID(i);
    int rssi = netMapper->getWiFiRSSI(i);
    uint8_t enc = netMapper->getWiFiEncryption(i);
    String lock = (enc != WIFI_AUTH_OPEN) ? "🔒" : "🔓";
    char item[30];
    sprintf(item, "%s %s %ddBm", lock.c_str(), ssid.substring(0, 12).c_str(), rssi);
    items[i] = String(item);
  }
  
  const char* itemPtrs[20];
  for (int i = 0; i < count && i < 20; i++) itemPtrs[i] = items[i].c_str();
  
  drawScrollingMenu(itemPtrs, count, ui.wifiNetworkIndex, 5, "Select Network", 12);
  
  if (isButtonPressed(BTN_UP)) ui.wifiNetworkIndex = (ui.wifiNetworkIndex - 1 + count) % count;
  if (isButtonPressed(BTN_DOWN)) ui.wifiNetworkIndex = (ui.wifiNetworkIndex + 1) % count;
  if (isButtonPressed(BTN_SELECT)) {
    uint8_t enc = netMapper->getWiFiEncryption(ui.wifiNetworkIndex);
    if (enc != WIFI_AUTH_OPEN) {
      ui.password[0] = '\0';
      ui.passwordPos = 0;
      ui.selectedIndex = 0;
      ui.shiftMode = false;
      ui.symbolMode = false;
      ui.keyboardPage = 0;
      ui.editingPassword = true;
      ui.menuState = NET_MENU_PASSWORD_ENTRY;
    } else {
      netMapper->connectToNetwork(ui.wifiNetworkIndex, "");
      ui.menuState = NET_MENU_MAIN;
      showPopup("Connected!", netMapper->getWiFiSSID(ui.wifiNetworkIndex).c_str());
    }
  }
  if (isButtonPressed(BTN_BACK)) ui.menuState = NET_MENU_MAIN;
}

// =================================================
// DEVICE LIST
// =================================================

static void handleDeviceList() {
  int count = netMapper->getDeviceCount();
  
  if (count == 0) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(25, "No devices found");
    CenteredStr(40, "SEL: Scan  BACK: Menu");
    u8g2.sendBuffer();
    if (isButtonPressed(BTN_SELECT)) { ui.menuState = NET_MENU_MAIN; ui.selectedIndex = 0; }
    if (isButtonPressed(BTN_BACK)) ui.menuState = NET_MENU_MAIN;
    return;
  }
  
  String items[20];
  for (int i = 0; i < count && i < 20; i++) {
    NetworkDevice* dev = netMapper->getDevice(i);
    if (dev) {
      char item[30];
      sprintf(item, "%s %s", dev->ip, dev->vendor);
      items[i] = String(item);
    }
  }
  
  const char* itemPtrs[20];
  for (int i = 0; i < count && i < 20; i++) itemPtrs[i] = items[i].c_str();
  
  drawScrollingMenu(itemPtrs, count, ui.selectedIndex, 5, "Devices", 12);
  
  if (isButtonPressed(BTN_UP)) ui.selectedIndex = (ui.selectedIndex - 1 + count) % count;
  if (isButtonPressed(BTN_DOWN)) ui.selectedIndex = (ui.selectedIndex + 1) % count;
  if (isButtonPressed(BTN_SELECT)) ui.menuState = NET_MENU_DEVICE_INFO;
  if (isButtonPressed(BTN_BACK)) ui.menuState = NET_MENU_MAIN;
}

// =================================================
// DEVICE INFO
// =================================================

static void handleDeviceInfo() {
  NetworkDevice* dev = netMapper->getDevice(ui.selectedIndex);
  if (!dev) { ui.menuState = NET_MENU_DEVICES; return; }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawXBMP(56, 0, 16, 16, icon_wifi);
  u8g2.drawLine(0, 12, 128, 12);
  
  u8g2.setCursor(0, 22);
  u8g2.print("IP: "); u8g2.print(dev->ip);
  
  u8g2.setCursor(0, 32);
  u8g2.print("MAC: "); 
  String mac = String(dev->mac);
  if (mac.length() > 12) u8g2.print(mac.substring(0, 12).c_str());
  else u8g2.print(mac.c_str());
  
  u8g2.setCursor(0, 42);
  u8g2.print("Vendor: "); u8g2.print(dev->vendor);
  
  u8g2.setCursor(0, 52);
  u8g2.print("Host: "); u8g2.print(dev->hostname);
  
  if (dev->openPortCount > 0) {
    u8g2.setCursor(0, 60);
    u8g2.print("Ports: ");
    for (int i = 0; i < dev->openPortCount && i < 4; i++) {
      u8g2.print(dev->openPorts[i]);
      if (i < dev->openPortCount - 1 && i < 3) u8g2.print(",");
    }
    if (dev->openPortCount > 4) u8g2.print("...");
  }
  
  if (dev->isVulnerable) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 62, 128, 2);
    u8g2.setDrawColor(1);
    u8g2.setCursor(0, 63);
    u8g2.print("⚠️ "); 
    String vuln = String(dev->vulnerability);
    if (vuln.length() > 18) vuln = vuln.substring(0, 16) + "..";
    u8g2.print(vuln.c_str());
  }
  
  u8g2.sendBuffer();
  
  if (isButtonPressed(BTN_BACK)) ui.menuState = NET_MENU_DEVICES;
  if (isButtonPressed(BTN_SELECT)) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    CenteredStr(25, "Rescanning Ports...");
    u8g2.sendBuffer();
    netMapper->scanPorts(dev);
    showPopup("Port Scan Complete", String(dev->openPortCount + " ports open").c_str());
  }
}

// =================================================
// MANUAL IP - 4 BUTTONS
// =================================================

static void handleManualIP() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
  u8g2.drawStr(15, 9, "Manual IP Scan");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 12, 128, 12);
  
  u8g2.setCursor(10, 28);
  u8g2.print("IP: ");
  
  String ipStr = String(ui.manualIP);
  int cursorX = 40;
  
  for (int i = 0; i < ipStr.length(); i++) {
    if (ui.editingIP && i == ui.manualIPPos) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(cursorX - 1, 20, 7, 10);
      u8g2.setDrawColor(1);
      u8g2.setCursor(cursorX, 28);
      u8g2.print(ipStr.charAt(i));
    } else {
      u8g2.setCursor(cursorX, 28);
      u8g2.print(ipStr.charAt(i));
    }
    cursorX += 6;
  }
  
  if (ui.editingIP) u8g2.drawFrame(38, 18, ipStr.length() * 6 + 4, 12);
  
  u8g2.setCursor(10, 42);
  if (ui.editingIP) {
    u8g2.print("UP/DN: Change digit");
    u8g2.setCursor(10, 50);
    u8g2.print("SEL+UP: Move Left");
    u8g2.setCursor(10, 58);
    u8g2.print("SEL+DN: Move Right");
  } else {
    u8g2.print("SEL: Edit IP");
    u8g2.setCursor(10, 50);
    u8g2.print("SEL again: Scan");
    u8g2.setCursor(10, 58);
    u8g2.print("BACK: Menu");
  }
  
  u8g2.sendBuffer();
  
  static unsigned long lastButtonTime = 0;
  unsigned long now = millis();
  
  if (now - lastButtonTime < 150) {
    delay(10);
    return;
  }
  
  if (isButtonPressed(BTN_SELECT) && !isButtonPressed(BTN_UP) && !isButtonPressed(BTN_DOWN)) {
    lastButtonTime = now;
    
    if (ui.editingIP) {
      ui.editingIP = false;
      
      int dots = 0;
      bool valid = true;
      int len = strlen(ui.manualIP);
      if (len < 7 || len > 15) valid = false;
      
      for (int i = 0; i < len && valid; i++) {
        if (ui.manualIP[i] == '.') dots++;
        else if (ui.manualIP[i] < '0' || ui.manualIP[i] > '9') valid = false;
      }
      
      if (dots != 3 || !valid) {
        showPopup("Invalid IP", "Format: 192.168.1.1");
        strcpy(ui.manualIP, "192.168.1.1");
        ui.manualIPPos = 0;
        return;
      }
      
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      CenteredStr(25, "Scanning IP...");
      char ipMsg[30];
      sprintf(ipMsg, "IP: %s", ui.manualIP);
      CenteredStr(40, ipMsg);
      u8g2.sendBuffer();
      
      bool found = netMapper->scanSpecificIP(ui.manualIP);
      
      if (found) {
        ui.menuState = NET_MENU_DEVICES;
        ui.selectedIndex = 0;
        showPopup("✅ Device Found!", ui.manualIP);
      } else {
        showPopup("❌ No Device", "IP not responding");
      }
    } else {
      ui.editingIP = true;
      ui.manualIPPos = 0;
    }
    return;
  }
  
  if (isButtonPressed(BTN_BACK)) {
    lastButtonTime = now;
    if (ui.editingIP) ui.editingIP = false;
    else ui.menuState = NET_MENU_MAIN;
    return;
  }
  
  if (ui.editingIP) {
    int len = strlen(ui.manualIP);
    
    // UP - Increase digit
    if (isButtonPressed(BTN_UP)) {
      lastButtonTime = now;
      char c = ui.manualIP[ui.manualIPPos];
      if (c >= '0' && c <= '9') {
        if (c < '9') ui.manualIP[ui.manualIPPos] = c + 1;
        else ui.manualIP[ui.manualIPPos] = '0';
      } else if (c == '.') {
        ui.manualIPPos++;
        if (ui.manualIPPos >= len) ui.manualIPPos = 0;
      }
    }
    
    // DOWN - Decrease digit
    if (isButtonPressed(BTN_DOWN)) {
      lastButtonTime = now;
      char c = ui.manualIP[ui.manualIPPos];
      if (c >= '0' && c <= '9') {
        if (c > '0') ui.manualIP[ui.manualIPPos] = c - 1;
        else ui.manualIP[ui.manualIPPos] = '9';
      } else if (c == '.') {
        ui.manualIPPos++;
        if (ui.manualIPPos >= len) ui.manualIPPos = 0;
      }
    }
    
    // MOVE LEFT: SELECT + UP
    if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_UP)) {
      lastButtonTime = now;
      ui.manualIPPos--;
      if (ui.manualIPPos < 0) ui.manualIPPos = len - 1;
      while (ui.manualIPPos >= 0 && ui.manualIP[ui.manualIPPos] == '.') ui.manualIPPos--;
      if (ui.manualIPPos < 0) ui.manualIPPos = len - 1;
    }
    
    // MOVE RIGHT: SELECT + DOWN
    if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_DOWN)) {
      lastButtonTime = now;
      ui.manualIPPos++;
      if (ui.manualIPPos >= len) ui.manualIPPos = 0;
      while (ui.manualIPPos < len && ui.manualIP[ui.manualIPPos] == '.') ui.manualIPPos++;
      if (ui.manualIPPos >= len) ui.manualIPPos = 0;
    }
  }
}

// =================================================
// PORT SCAN
// =================================================

static void handlePortScan() {
  int count = netMapper->getDeviceCount();
  
  if (count == 0) {
    showPopup("No devices", "Scan first!");
    ui.menuState = NET_MENU_MAIN;
    return;
  }
  
  String items[20];
  for (int i = 0; i < count && i < 20; i++) {
    NetworkDevice* dev = netMapper->getDevice(i);
    if (dev) {
      char item[30];
      sprintf(item, "%s %s", dev->ip, dev->vendor);
      items[i] = String(item);
    }
  }
  
  const char* itemPtrs[20];
  for (int i = 0; i < count && i < 20; i++) itemPtrs[i] = items[i].c_str();
  
  drawScrollingMenu(itemPtrs, count, ui.selectedIndex, 5, "Select Device", 12);
  
  if (isButtonPressed(BTN_UP)) ui.selectedIndex = (ui.selectedIndex - 1 + count) % count;
  if (isButtonPressed(BTN_DOWN)) ui.selectedIndex = (ui.selectedIndex + 1) % count;
  if (isButtonPressed(BTN_SELECT)) {
    NetworkDevice* dev = netMapper->getDevice(ui.selectedIndex);
    if (dev) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      CenteredStr(25, "Scanning Ports...");
      char ipMsg[30];
      sprintf(ipMsg, "IP: %s", dev->ip);
      CenteredStr(40, ipMsg);
      u8g2.sendBuffer();
      netMapper->scanPorts(dev);
      ui.menuState = NET_MENU_DEVICE_INFO;
    }
  }
  if (isButtonPressed(BTN_BACK)) ui.menuState = NET_MENU_MAIN;
}

// =================================================
// EXPORT
// =================================================

static void handleExport() {
  netMapper->exportToJSON();
  showPopup("Exported", "Check Serial Monitor!");
  ui.menuState = NET_MENU_MAIN;
}

// =================================================
// SETUP AND LOOP
// =================================================

void networkMapperSetup() {
  if (!netMapper) netMapper = new NetworkMapper(MAX_DEVICES);
  
  ui.menuState = NET_MENU_MAIN;
  ui.selectedIndex = 0;
  ui.scrollOffset = 0;
  ui.editingIP = false;
  ui.scanning = false;
  ui.showProgress = false;
  ui.lastUpdate = 0;
  ui.wifiNetworkIndex = 0;
  ui.inNetworkSelect = false;
  strcpy(ui.manualIP, "192.168.1.1");
  ui.manualIPPos = 0;
  ui.scanStart = 1;
  ui.scanEnd = 254;
  ui.password[0] = '\0';
  ui.passwordPos = 0;
  ui.editingPassword = false;
  ui.shiftMode = false;
  ui.symbolMode = false;
  ui.keyboardPage = 0;
  
  netMapper->scanWiFiNetworks();
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBMP(56, 5, 16, 16, icon_wifi);
  CenteredStr(20, "Network Mapper");
  
  char netCount[32];
  sprintf(netCount, "Networks: %d", netMapper->getWiFiNetworkCount());
  CenteredStr(35, netCount);
  CenteredStr(50, "SELECT: Scan");
  CenteredStr(63, "BACK: Exit");
  u8g2.sendBuffer();
  
  Serial.println("📡 Network Mapper ready");
}

void networkMapperLoop() {
  if (!netMapper) return;
  
  if (isButtonPressed(BTN_BACK)) {
    if (netMapper->isScanning()) netMapper->stopScan();
    if (ui.menuState == NET_MENU_PASSWORD_ENTRY) {
      ui.password[0] = '\0';
      ui.passwordPos = 0;
      ui.shiftMode = false;
      ui.menuState = NET_MENU_SELECT_NETWORK;
      return;
    }
    exitModule();
    return;
  }
  
  switch (ui.menuState) {
    case NET_MENU_MAIN: handleMainMenu(); break;
    case NET_MENU_SCAN_WIFI: handleWiFiScan(); break;
    case NET_MENU_SELECT_NETWORK: handleNetworkSelect(); break;
    case NET_MENU_PASSWORD_ENTRY: handlePasswordEntry(); break;
    case NET_MENU_DEVICES: handleDeviceList(); break;
    case NET_MENU_DEVICE_INFO: handleDeviceInfo(); break;
    case NET_MENU_MANUAL_IP: handleManualIP(); break;
    case NET_MENU_PORT_SCAN: handlePortScan(); break;
    case NET_MENU_EXPORT: handleExport(); break;
  }
  
  delay(20);
}

// =================================================
// CLEANUP
// =================================================

void networkMapperCleanup() {
  Serial.println("🧹 Network Mapper cleanup...");
  if (netMapper) {
    if (netMapper->isScanning()) netMapper->stopScan();
    delete netMapper;
    netMapper = nullptr;
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  delay(100);
  ui.menuState = NET_MENU_MAIN;
  ui.selectedIndex = 0;
  ui.scanning = false;
  Serial.println("✅ Network Mapper cleanup complete");
}