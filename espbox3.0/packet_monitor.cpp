#include "packet_monitor.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// =================================================
// STATIC INSTANCE
// =================================================
static PacketMonitor* monitorInstance = nullptr;
static PacketMonitor* packetMon = nullptr;

// =================================================
// MENU STATE
// =================================================
enum ViewMode {
    VIEW_MAIN,
    VIEW_SPECTRUM,
    VIEW_PACKETS,
    VIEW_PACKET_DETAIL
};

static ViewMode currentView = VIEW_MAIN;
static int packetMenuIndex = 0;
static int packetViewIndex = 0;
static bool showLogStatus = false;
static unsigned long lastLogUpdate = 0;

// =================================================
// HELPER FUNCTION
// =================================================

String getPacketType(PacketData* pkt) {
  if (!pkt) return "Unknown";
  switch(pkt->type) {
    case 0:
      switch(pkt->subtype) {
        case 0x80: return "Beacon";
        case 0x40: return "ProbeReq";
        case 0x50: return "ProbeResp";
        case 0xC0: return "Deauth";
        case 0xA0: return "Disassoc";
        case 0xD0: return "Action";
        default: return "Mgmt";
      }
    case 1: return "Control";
    case 2: return "Data";
    default: return "Unknown";
  }
}

// =================================================
// PACKET MONITOR IMPLEMENTATION
// =================================================

PacketMonitor::PacketMonitor(uint16_t size) {
  bufferSize = size;
  packetBuffer = new PacketData[bufferSize];
  packetCount = 0;
  packetIndex = 0;
  currentFilter = PROTO_ALL;
  captureActive = false;
  filterEnabled = false;
  totalPackets = 0;
  filteredPackets = 0;
  currentLogFile = "";
  monitorInstance = this;
  
  for (int ch = 1; ch <= CHANNELS; ch++) {
    spectrum.rssi[ch] = -90;
    spectrum.smooth[ch] = -90;
    spectrum.sum[ch] = 0;
    spectrum.count[ch] = 0;
  }
  spectrum.current_channel = 1;
  spectrum.best_channel = 1;
  spectrum.total_packets = 0;
  spectrum.scan_count = 0;
  channelStartTime = 0;
  lastDisplayUpdate = 0;
  firstDraw = true;
}

PacketMonitor::~PacketMonitor() {
  if (packetBuffer) {
    delete[] packetBuffer;
    packetBuffer = nullptr;
  }
  closeLogFile();
}

// =================================================
// SPECTRUM FUNCTIONS
// =================================================

void PacketMonitor::updateSpectrumChannel() {
  unsigned long now = millis();
  if (now - channelStartTime < SCAN_TIME_MS) return;
  
  if (spectrum.count[spectrum.current_channel] > 0) {
    spectrum.rssi[spectrum.current_channel] = 
      spectrum.sum[spectrum.current_channel] / spectrum.count[spectrum.current_channel];
    spectrum.smooth[spectrum.current_channel] = 
      (int)((spectrum.smooth[spectrum.current_channel] * 0.6) + 
            (spectrum.rssi[spectrum.current_channel] * 0.4));
  } else {
    spectrum.smooth[spectrum.current_channel] = 
      (int)((spectrum.smooth[spectrum.current_channel] * 0.6) + (-90 * 0.4));
  }
  
  spectrum.sum[spectrum.current_channel] = 0;
  spectrum.count[spectrum.current_channel] = 0;
  spectrum.current_channel = (spectrum.current_channel % CHANNELS) + 1;
  esp_wifi_set_channel(spectrum.current_channel, WIFI_SECOND_CHAN_NONE);
  channelStartTime = now;
  
  if (spectrum.current_channel == 1) {
    spectrum.scan_count++;
    spectrum.best_channel = 1;
    for (int ch = 2; ch <= CHANNELS; ch++) {
      if (spectrum.smooth[ch] > spectrum.smooth[spectrum.best_channel]) {
        spectrum.best_channel = ch;
      }
    }
  }
}

// =================================================
// DRAW SPECTRUM
// =================================================

void PacketMonitor::drawSpectrum() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(2, 0, 10, 10, icon_wifi);
  u8g2.drawStr(15, 8, "📡 SPECTRUM");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 10, 128, 10);
  
  u8g2.setCursor(90, 8);
  u8g2.print(captureActive ? "🔴" : "⚪");
  u8g2.setCursor(105, 8);
  u8g2.print("CH");
  u8g2.print(spectrum.current_channel);
  
  int graphY = 12;
  int graphHeight = 46;
  int graphBottom = graphY + graphHeight;
  int barWidth = 8;
  int barSpacing = 1;
  int totalBarWidth = barWidth + barSpacing;
  int startX = 1;
  
  u8g2.drawFrame(0, graphY, 128, graphHeight);
  
  u8g2.setDrawColor(0);
  u8g2.drawLine(0, graphY + graphHeight/2, 128, graphY + graphHeight/2);
  u8g2.drawLine(0, graphY + graphHeight/4, 128, graphY + graphHeight/4);
  u8g2.drawLine(0, graphY + (graphHeight*3)/4, 128, graphY + (graphHeight*3)/4);
  u8g2.setDrawColor(1);
  
  int min_rssi = -90;
  int max_rssi = -30;
  for (int ch = 1; ch <= CHANNELS; ch++) {
    if (spectrum.smooth[ch] > max_rssi) max_rssi = spectrum.smooth[ch];
    if (spectrum.smooth[ch] < min_rssi) min_rssi = spectrum.smooth[ch];
  }
  if (max_rssi - min_rssi < 15) min_rssi = max_rssi - 25;
  int range = max_rssi - min_rssi;
  if (range < 5) range = 10;
  
  for (int ch = 1; ch <= CHANNELS; ch++) {
    int rssi = spectrum.smooth[ch];
    int height = map(rssi, min_rssi, max_rssi, 2, graphHeight - 2);
    if (height < 2) height = 2;
    if (height > graphHeight - 2) height = graphHeight - 2;
    
    int x = startX + (ch - 1) * totalBarWidth;
    if (x > 128 - barWidth) break;
    int y = graphBottom - 1 - height;
    
    if (rssi > -50) {
      u8g2.drawBox(x, y, barWidth, height);
      u8g2.drawFrame(x, y, barWidth, height);
    } else if (rssi > -65) {
      u8g2.drawBox(x, y, barWidth, height);
    } else if (rssi > -75) {
      u8g2.drawBox(x, y, barWidth, height);
      u8g2.setDrawColor(0);
      for (int py = y + 2; py < graphBottom - 2; py += 4) {
        u8g2.drawLine(x + 1, py, x + barWidth - 2, py);
      }
      u8g2.setDrawColor(1);
    } else if (rssi > -85) {
      u8g2.drawFrame(x, y, barWidth, height);
    } else {
      u8g2.drawLine(x + 1, graphBottom - 2, x + barWidth - 2, graphBottom - 2);
    }
  }
  
  u8g2.setFont(u8g2_font_4x6_tf);
  for (int ch = 1; ch <= CHANNELS; ch++) {
    if (ch % 2 == 1) {
      int x_pos = startX + (ch - 1) * totalBarWidth + 2;
      if (x_pos < 125) {
        u8g2.setCursor(x_pos, graphBottom + 6);
        u8g2.print(ch);
      }
    }
  }
  
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.setCursor(1, graphY + 5);
  u8g2.print("-30");
  u8g2.setCursor(1, graphY + graphHeight/2 + 2);
  u8g2.print("-60");
  u8g2.setCursor(1, graphBottom - 2);
  u8g2.print("-90");
  
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(2, 62);
  u8g2.print("Best CH");
  u8g2.print(spectrum.best_channel);
  u8g2.print(" ");
  u8g2.print(spectrum.smooth[spectrum.best_channel]);
  u8g2.print("dBm");
  
  u8g2.setCursor(95, 62);
  u8g2.print("Scan:");
  u8g2.print(spectrum.scan_count);
  
  u8g2.sendBuffer();
}

// =================================================
// PACKET VIEW
// =================================================

void PacketMonitor::drawPacketView() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(2, 0, 10, 10, icon_wifi);
  u8g2.drawStr(15, 8, "📦 Packets");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 10, 128, 10);
  
  u8g2.setCursor(80, 8);
  u8g2.print(captureActive ? "🔴" : "⚪");
  
  if (packetCount == 0) {
    u8g2.setCursor(2, 25);
    u8g2.print("No packets captured");
    u8g2.setCursor(2, 35);
    u8g2.print("Press SELECT to start");
    u8g2.sendBuffer();
    return;
  }
  
  int visible = 4;
  int startIdx = 0;
  if (packetViewIndex >= visible) startIdx = packetViewIndex - visible + 1;
  if (startIdx > packetCount - visible) startIdx = packetCount - visible;
  if (startIdx < 0) startIdx = 0;
  
  for (int i = 0; i < visible && (startIdx + i) < packetCount; i++) {
    int idx = startIdx + i;
    int y = 14 + i * 11;
    
    PacketData* pkt = getPacket(idx);
    if (!pkt) continue;
    
    if (idx == packetViewIndex) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(0, y - 1, 128, 10);
      u8g2.setDrawColor(1);
      u8g2.setCursor(2, y + 1);
      u8g2.print(">");
      u8g2.setCursor(12, y + 1);
    } else {
      u8g2.setCursor(2, y + 1);
      u8g2.print(" ");
      u8g2.setCursor(12, y + 1);
    }
    
    u8g2.print(getPacketSummary(idx));
  }
  
  if (startIdx > 0) u8g2.setCursor(120, 12);
  if (startIdx + visible < packetCount) u8g2.setCursor(120, 60);
  
  u8g2.setCursor(2, 63);
  u8g2.print("UP/DN:Nav  SEL:Detail");
  
  u8g2.sendBuffer();
}

// =================================================
// MAIN VIEW
// =================================================

void PacketMonitor::drawMainView() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(2, 1, 10, 10, icon_wifi);
  u8g2.drawStr(15, 9, captureActive ? "🔴 CAPTURING" : "⚪ IDLE");
  u8g2.setDrawColor(1);
  u8g2.drawLine(0, 12, 128, 12);
  
  char buf[30];
  sprintf(buf, "Total: %d", totalPackets);
  u8g2.setCursor(2, 22);
  u8g2.print(buf);
  
  sprintf(buf, "Stored: %d", packetCount);
  u8g2.setCursor(2, 32);
  u8g2.print(buf);
  
  if (captureActive && isLogging()) {
    u8g2.setCursor(2, 42);
    String logFile = getCurrentLogFile();
    if (logFile.length() > 14) {
      u8g2.print("📁 " + logFile.substring(0, 12) + "..");
    } else {
      u8g2.print("📁 " + logFile);
    }
  }
  
  sprintf(buf, "Best CH: %d (%ddBm)", 
          spectrum.best_channel, 
          spectrum.smooth[spectrum.best_channel]);
  u8g2.setCursor(2, 52);
  u8g2.print(buf);
  
  u8g2.setCursor(2, 63);
  u8g2.print("SEL:Start/Stop  SEL+UP:View");
  
  u8g2.sendBuffer();
}

// =================================================
// LOG FILE MANAGEMENT
// =================================================

void PacketMonitor::openLogFile() {
  if (!SPIFFS.begin(true)) return;
  String timestamp = String(millis() / 1000);
  currentLogFile = "/packets_" + timestamp + ".txt";
  
  if (!SPIFFS.exists(currentLogFile)) {
    File file = SPIFFS.open(currentLogFile, "w");
    if (file) {
      file.println("========================================");
      file.println("PACKET CAPTURE LOG");
      file.println("Started: " + String(millis() / 1000) + "s");
      file.println("========================================");
      file.println();
      file.close();
    }
  }
}

void PacketMonitor::closeLogFile() {
  if (currentLogFile.length() > 0) {
    if (SPIFFS.begin(true)) {
      File file = SPIFFS.open(currentLogFile, "a");
      if (file) {
        file.println();
        file.println("========================================");
        file.println("CAPTURE STOPPED");
        file.println("Total Packets: " + String(totalPackets));
        file.println("Stored Packets: " + String(packetCount));
        file.println("========================================");
        file.close();
      }
    }
    currentLogFile = "";
  }
}

void PacketMonitor::savePacketToLog(const PacketData& packet) {
  if (currentLogFile.length() == 0) return;
  if (!SPIFFS.begin(true)) return;
  
  File file = SPIFFS.open(currentLogFile, "a");
  if (!file) return;
  
  file.print("[");
  file.print(packet.timestamp);
  file.print("] ");
  
  switch(packet.type) {
    case 0: file.print("MGMT"); break;
    case 1: file.print("CTRL"); break;
    case 2: file.print("DATA"); break;
    default: file.print("UNKN");
  }
  
  file.print(" | ");
  file.print(packet.subtype, HEX);
  file.print(" | ");
  file.print(packet.source_mac);
  file.print(" -> ");
  file.print(packet.dest_mac);
  file.print(" | ");
  file.print(packet.rssi);
  file.print("dBm | CH");
  file.print(packet.channel);
  
  if (packet.ssid.length() > 0) {
    file.print(" | ");
    file.print(packet.ssid);
  }
  if (packet.info.length() > 0) {
    file.print(" | ");
    file.print(packet.info);
  }
  
  file.println();
  file.close();
}

void PacketMonitor::startLogging() {
  if (currentLogFile.length() == 0) openLogFile();
}

void PacketMonitor::stopLogging() {
  closeLogFile();
}

bool PacketMonitor::isLogging() {
  return currentLogFile.length() > 0;
}

String PacketMonitor::getCurrentLogFile() {
  return currentLogFile;
}

void PacketMonitor::setLogFile(String filename) {
  if (!filename.startsWith("/")) filename = "/" + filename;
  if (!filename.endsWith(".txt")) filename += ".txt";
  currentLogFile = filename;
}

// =================================================
// CAPTURE FUNCTIONS
// =================================================

bool PacketMonitor::startCapture() {
  if (captureActive) return true;
  
  Serial.println("📡 Starting Packet Capture...");
  startLogging();
  
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
  
  esp_wifi_set_channel(spectrum.current_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&PacketMonitor::packetCallback);
  
  captureActive = true;
  totalPackets = 0;
  filteredPackets = 0;
  packetCount = 0;
  packetIndex = 0;
  channelStartTime = millis();
  
  Serial.println("✅ Packet Capture Started!");
  Serial.println("📁 Logging to: " + currentLogFile);
  return true;
}

void PacketMonitor::stopCapture() {
  if (!captureActive) return;
  
  Serial.println("⏹️ Stopping Packet Capture...");
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  captureActive = false;
  stopLogging();
  
  Serial.printf("📊 Captured: %d packets\n", totalPackets);
  Serial.printf("📁 Saved to: %s\n", currentLogFile.c_str());
}

void PacketMonitor::clearBuffer() {
  packetCount = 0;
  packetIndex = 0;
  totalPackets = 0;
  filteredPackets = 0;
}

void PacketMonitor::setFilter(ProtocolFilter filter) {
  currentFilter = filter;
  filterEnabled = true;
}

void PacketMonitor::enableFilter(bool enable) {
  filterEnabled = enable;
}

bool PacketMonitor::shouldStorePacket(const PacketData& packet) {
  if (!filterEnabled || currentFilter == PROTO_ALL) return true;
  
  switch(currentFilter) {
    case PROTO_BEACON: return (packet.type == 0 && packet.subtype == 0x80);
    case PROTO_DATA: return (packet.type == 2);
    case PROTO_PROBE_REQ: return (packet.type == 0 && packet.subtype == 0x40);
    case PROTO_PROBE_RESP: return (packet.type == 0 && packet.subtype == 0x50);
    case PROTO_DEAUTH: return (packet.type == 0 && packet.subtype == 0xC0);
    case PROTO_DISASSOC: return (packet.type == 0 && packet.subtype == 0xA0);
    case PROTO_ACTION: return (packet.type == 0 && packet.subtype == 0xD0);
    default: return true;
  }
}

String PacketMonitor::macToString(const uint8_t* mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void PacketMonitor::decodeBeacon(const uint8_t* payload, uint16_t len, PacketData* packet) {
  uint16_t offset = 12;
  while (offset + 2 < len) {
    uint8_t tag = payload[offset];
    uint8_t tagLen = payload[offset + 1];
    if (tag == 0 && tagLen > 0 && tagLen <= 32) {
      char ssid[33];
      memcpy(ssid, &payload[offset + 2], tagLen);
      ssid[tagLen] = '\0';
      packet->ssid = String(ssid);
      if (packet->ssid.length() == 0) packet->ssid = "<Hidden>";
      break;
    }
    offset += 2 + tagLen;
    if (offset >= len) break;
  }
}

void PacketMonitor::decodeProbeReq(const uint8_t* payload, uint16_t len, PacketData* packet) {
  uint16_t offset = 0;
  while (offset + 2 < len) {
    uint8_t tag = payload[offset];
    uint8_t tagLen = payload[offset + 1];
    if (tag == 0 && tagLen > 0 && tagLen <= 32) {
      char ssid[33];
      memcpy(ssid, &payload[offset + 2], tagLen);
      ssid[tagLen] = '\0';
      packet->ssid = String(ssid);
      if (packet->ssid.length() == 0) packet->ssid = "<Broadcast>";
      break;
    }
    offset += 2 + tagLen;
    if (offset >= len) break;
  }
}

void PacketMonitor::decodeDeauth(const uint8_t* payload, uint16_t len, PacketData* packet) {
  if (len >= 2) {
    uint16_t reason = payload[0] | (payload[1] << 8);
    String reasonStr;
    switch(reason) {
      case 1: reasonStr = "Unspecified"; break;
      case 2: reasonStr = "Prev auth invalid"; break;
      case 3: reasonStr = "Station leaving"; break;
      case 4: reasonStr = "Inactivity"; break;
      case 5: reasonStr = "AP busy"; break;
      default: reasonStr = "Reason " + String(reason);
    }
    packet->info = "Reason: " + reasonStr;
  }
}

// =================================================
// PACKET CALLBACK
// =================================================

void PacketMonitor::packetCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!monitorInstance || !monitorInstance->captureActive) return;
  
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  monitorInstance->totalPackets++;
  
  uint8_t* payload = pkt->payload;
  uint16_t len = pkt->rx_ctrl.sig_len;
  
  if (len < 24) return;
  
  uint16_t frameCtrl = payload[0] | (payload[1] << 8);
  uint8_t pktType = (frameCtrl >> 2) & 0x03;
  uint8_t subtype = (frameCtrl >> 4) & 0x0F;
  
  int channel = pkt->rx_ctrl.channel;
  int rssi = pkt->rx_ctrl.rssi;
  if (channel >= 1 && channel <= CHANNELS && rssi > -100 && rssi < -20) {
    monitorInstance->spectrum.sum[channel] += rssi;
    monitorInstance->spectrum.count[channel]++;
  }
  
  PacketData newPacket;
  newPacket.type = pktType;
  newPacket.subtype = subtype;
  newPacket.channel = channel;
  newPacket.rssi = rssi;
  newPacket.timestamp = millis();
  newPacket.ssid = "";
  newPacket.info = "";
  newPacket.decoded = false;
  
  uint8_t* addr1 = &payload[4];
  uint8_t* addr2 = &payload[10];
  uint8_t* addr3 = &payload[16];
  
  monitorInstance->macToString(addr2).toCharArray(newPacket.source_mac, 18);
  monitorInstance->macToString(addr1).toCharArray(newPacket.dest_mac, 18);
  monitorInstance->macToString(addr3).toCharArray(newPacket.bssid_mac, 18);
  
  if (pktType == 0) {
    switch(subtype) {
      case 0x80:
        newPacket.ssid = "BEACON";
        if (len > 36) monitorInstance->decodeBeacon(&payload[24], len - 24, &newPacket);
        break;
      case 0x40:
        newPacket.ssid = "PROBE_REQ";
        if (len > 24) monitorInstance->decodeProbeReq(&payload[24], len - 24, &newPacket);
        break;
      case 0x50: newPacket.ssid = "PROBE_RESP"; break;
      case 0xC0:
        newPacket.ssid = "DEAUTH";
        if (len > 24) monitorInstance->decodeDeauth(&payload[24], len - 24, &newPacket);
        break;
      case 0xA0: newPacket.ssid = "DISASSOC"; break;
      case 0xD0: newPacket.ssid = "ACTION"; break;
      default:
        char tmp[16];
        sprintf(tmp, "MGMT_%02X", subtype);
        newPacket.ssid = String(tmp);
    }
  } else if (pktType == 1) {
    newPacket.ssid = "CTRL";
  } else if (pktType == 2) {
    newPacket.ssid = "DATA";
    if (len > 24) {
      uint8_t* data = &payload[24];
      uint16_t dataLen = len - 24;
      if (dataLen > 10) {
        if (data[0] == 'G' && data[1] == 'E' && data[2] == 'T') newPacket.info = "HTTP GET";
        else if (data[0] == 'P' && data[1] == 'O' && data[2] == 'S' && data[3] == 'T') newPacket.info = "HTTP POST";
      }
    }
  }
  
  if (monitorInstance->shouldStorePacket(newPacket)) {
    uint16_t idx = monitorInstance->packetCount % monitorInstance->bufferSize;
    monitorInstance->packetBuffer[idx] = newPacket;
    monitorInstance->packetCount++;
    monitorInstance->filteredPackets++;
    monitorInstance->savePacketToLog(newPacket);
  }
}

// =================================================
// PUBLIC METHODS
// =================================================

PacketData* PacketMonitor::getPacket(uint16_t index) {
  if (index >= packetCount) return nullptr;
  uint16_t idx = index % bufferSize;
  return &packetBuffer[idx];
}

uint16_t PacketMonitor::getPacketCount() { return packetCount; }
uint32_t PacketMonitor::getTotalPackets() { return totalPackets; }
uint32_t PacketMonitor::getFilteredPackets() { return filteredPackets; }
bool PacketMonitor::isActive() { return captureActive; }
SpectrumData* PacketMonitor::getSpectrumData() { return &spectrum; }
int PacketMonitor::getBestChannel() { return spectrum.best_channel; }

String PacketMonitor::getPacketInfo(uint16_t index) {
  PacketData* pkt = getPacket(index);
  if (!pkt) return "Invalid";
  
  String info = "Type: ";
  switch(pkt->type) {
    case 0: info += "Management"; break;
    case 1: info += "Control"; break;
    case 2: info += "Data"; break;
    default: info += "Unknown";
  }
  info += " | Subtype: 0x" + String(pkt->subtype, HEX);
  info += " | CH: " + String(pkt->channel);
  info += " | RSSI: " + String(pkt->rssi) + "dBm";
  if (pkt->ssid.length() > 0) info += " | SSID: " + pkt->ssid;
  return info;
}

String PacketMonitor::getPacketSummary(uint16_t index) {
  PacketData* pkt = getPacket(index);
  if (!pkt) return "";
  
  String summary = "";
  switch(pkt->type) {
    case 0: summary += "📡 "; break;
    case 1: summary += "🎮 "; break;
    case 2: summary += "📦 "; break;
    default: summary += "❓ ";
  }
  
  if (pkt->ssid.length() > 0 && pkt->ssid != "DATA") {
    if (pkt->ssid.length() > 12) summary += pkt->ssid.substring(0, 10) + "..";
    else summary += pkt->ssid;
  } else {
    summary += getPacketType(pkt);
  }
  summary += " | " + String(pkt->rssi) + "dBm";
  return summary;
}

// =================================================
// SETUP AND LOOP - 4 BUTTONS
// =================================================

void packetMonitorSetup() {
  if (!packetMon) {
    packetMon = new PacketMonitor(100);
  }
  packetMenuIndex = 0;
  packetViewIndex = 0;
  currentView = VIEW_MAIN;
  showLogStatus = false;
  
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed");
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawXBMP(56, 5, 16, 16, icon_wifi);
  CenteredStr(20, "Packet Monitor");
  CenteredStr(35, "SELECT: Start/Stop");
  CenteredStr(50, "SEL+UP: View");
  CenteredStr(63, "BACK: Exit");
  u8g2.sendBuffer();
}

// =================================================
// LOOP - 4 BUTTONS
// =================================================

void packetMonitorLoop() {
  if (isButtonPressed(BTN_BACK)) {
    if (packetMon && packetMon->isActive()) {
      packetMon->stopCapture();
    }
    exitModule();
    return;
  }
  
  if (!packetMon) return;
  
  packetMon->updateSpectrumChannel();
  
  // Toggle capture on SELECT (single press)
  if (isButtonPressed(BTN_SELECT) && !isButtonPressed(BTN_UP) && !isButtonPressed(BTN_DOWN)) {
    if (packetMon->isActive()) {
      packetMon->stopCapture();
    } else {
      packetMon->startCapture();
    }
  }
  
  // Cycle views: SELECT + UP
  if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_UP)) {
    switch(currentView) {
      case VIEW_MAIN: currentView = VIEW_SPECTRUM; break;
      case VIEW_SPECTRUM: currentView = VIEW_PACKETS; packetViewIndex = 0; break;
      case VIEW_PACKETS: currentView = VIEW_MAIN; break;
      default: currentView = VIEW_MAIN;
    }
  }
  
  // Navigation in packet view - UP/DOWN
  if (currentView == VIEW_PACKETS) {
    if (isButtonPressed(BTN_UP) && packetViewIndex > 0) packetViewIndex--;
    if (isButtonPressed(BTN_DOWN) && packetViewIndex < packetMon->getPacketCount() - 1) packetViewIndex++;
  }
  
  // Draw based on current view
  switch(currentView) {
    case VIEW_MAIN: packetMon->drawMainView(); break;
    case VIEW_SPECTRUM: packetMon->drawSpectrum(); break;
    case VIEW_PACKETS: packetMon->drawPacketView(); break;
    default: packetMon->drawMainView();
  }
  
  delay(15);
}

// =================================================
// CLEANUP
// =================================================

void packetMonitorCleanup() {
  Serial.println("🧹 Packet Monitor cleanup...");
  unsigned long startTime = millis();
  
  if (packetMon && packetMon->isActive()) {
    packetMon->stopCapture();
    delay(50);
  }
  
  if (packetMon) {
    packetMon->stopLogging();
  }
  
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  
  if (packetMon) {
    delete packetMon;
    packetMon = nullptr;
  }
  
  packetMenuIndex = 0;
  packetViewIndex = 0;
  currentView = VIEW_MAIN;
  
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  Serial.printf("✅ Packet Monitor cleanup complete (%lu ms)\n", millis() - startTime);
}