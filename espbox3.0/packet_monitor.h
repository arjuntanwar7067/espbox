#ifndef PACKET_MONITOR_H
#define PACKET_MONITOR_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <U8g2lib.h>

// =================================================
// PACKET DATA STRUCTURE
// =================================================
struct PacketData {
    uint8_t type;
    uint8_t subtype;
    uint8_t channel;
    int8_t rssi;
    uint16_t payload_length;
    char source_mac[18];
    char dest_mac[18];
    char bssid_mac[18];
    unsigned long timestamp;
    String ssid;
    String info;
    bool decoded;
};

// =================================================
// SPECTRUM DATA STRUCTURE
// =================================================
#define CHANNELS 14
#define SCAN_TIME_MS 150

struct SpectrumData {
    int rssi[CHANNELS + 1];
    int smooth[CHANNELS + 1];
    int sum[CHANNELS + 1];
    int count[CHANNELS + 1];
    int current_channel;
    int best_channel;
    int total_packets;
    int scan_count;
};

enum ProtocolFilter {
    PROTO_ALL = 0,
    PROTO_BEACON,
    PROTO_DATA,
    PROTO_PROBE_REQ,
    PROTO_PROBE_RESP,
    PROTO_DEAUTH,
    PROTO_DISASSOC,
    PROTO_ACTION
};

// =================================================
// PACKET MONITOR CLASS
// =================================================
class PacketMonitor {
  private:
    PacketData* packetBuffer;
    uint16_t bufferSize;
    uint16_t packetCount;
    uint16_t packetIndex;
    ProtocolFilter currentFilter;
    bool captureActive;
    bool filterEnabled;
    uint32_t totalPackets;
    uint32_t filteredPackets;
    String currentLogFile;
    
    SpectrumData spectrum;
    unsigned long channelStartTime;
    unsigned long lastDisplayUpdate;
    const unsigned long DISPLAY_INTERVAL = 80;
    bool firstDraw;
    
    void decodeBeacon(const uint8_t* payload, uint16_t len, PacketData* packet);
    void decodeProbeReq(const uint8_t* payload, uint16_t len, PacketData* packet);
    void decodeDeauth(const uint8_t* payload, uint16_t len, PacketData* packet);
    bool shouldStorePacket(const PacketData& packet);
    String macToString(const uint8_t* mac);
    void savePacketToLog(const PacketData& packet);
    void openLogFile();
    void closeLogFile();
    
  public:
    PacketMonitor(uint16_t size = 100);
    ~PacketMonitor();
    
    bool startCapture();
    void stopCapture();
    void clearBuffer();
    void setFilter(ProtocolFilter filter);
    void enableFilter(bool enable);
    PacketData* getPacket(uint16_t index);
    uint16_t getPacketCount();
    uint32_t getTotalPackets();
    uint32_t getFilteredPackets();
    bool isActive();
    
    void startLogging();
    void stopLogging();
    bool isLogging();
    String getCurrentLogFile();
    void setLogFile(String filename);
    
    SpectrumData* getSpectrumData();
    int getBestChannel();
    
    void updateSpectrumChannel();
    void drawSpectrum();
    void drawPacketView();
    void drawMainView();
    
    static void packetCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    
    String getPacketInfo(uint16_t index);
    String getPacketSummary(uint16_t index);
};

void packetMonitorSetup();
void packetMonitorLoop();
void packetMonitorCleanup();

#endif