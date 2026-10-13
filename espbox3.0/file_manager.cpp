#include "file_manager.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include "pins.h"
#include <U8g2lib.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <WiFi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern WebServer webServer;

// =================================================
// STATE
// =================================================
int fmMenuIndex = 0;
int fmFileIndex = 0;
int fmScrollOffset = 0;
String fmFiles[30];
int fmFileCount = 0;
String fmCurrentPath = "/";
bool fmWebEnabled = false;
bool fmAPStarted = false;

// =================================================
// OWN WEB SERVER FOR FILE MANAGER
// =================================================
WebServer fmWebServer(80);

// =================================================
// FILE TYPE ICONS
// =================================================
const char* getFileIcon(String filename) {
    if (filename.endsWith(".json")) return "📄";
    if (filename.endsWith(".txt")) return "📝";
    if (filename.endsWith(".html")) return "🌐";
    if (filename.endsWith(".cap")) return "🔓";
    if (filename.endsWith(".pcap")) return "🔓";
    if (filename.endsWith(".jpg") || filename.endsWith(".png")) return "🖼️";
    if (filename.endsWith(".mp3")) return "🎵";
    if (filename.endsWith(".bin")) return "⚙️";
    if (filename.endsWith(".csv")) return "📊";
    if (filename.endsWith(".log")) return "📋";
    return "📄";
}

// =================================================
// LIST FILES
// =================================================
void listSpiFFS(String path) {
    fmFileCount = 0;
    
    if (!path.endsWith("/")) path += "/";
    
    File root = SPIFFS.open(path);
    if (!root) {
        Serial.println("❌ Cannot open: " + path);
        return;
    }
    if (!root.isDirectory()) { 
        root.close(); 
        return; 
    }
    
    File file = root.openNextFile();
    while (file && fmFileCount < 30) {
        String name = file.name();
        if (name.startsWith("/")) name = name.substring(1);
        
        if (file.isDirectory()) {
            name += "/";
        } else {
            long size = file.size();
            if (size < 1024) name += " (" + String(size) + "B)";
            else if (size < 1024 * 1024) name += " (" + String(size / 1024) + "KB)";
            else name += " (" + String(size / (1024 * 1024)) + "MB)";
        }
        
        fmFiles[fmFileCount++] = name;
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    // Sort files - directories first
    for (int i = 0; i < fmFileCount - 1; i++) {
        for (int j = i + 1; j < fmFileCount; j++) {
            bool dirI = fmFiles[i].endsWith("/");
            bool dirJ = fmFiles[j].endsWith("/");
            
            if (dirI && !dirJ) continue;
            if (!dirI && dirJ) {
                String temp = fmFiles[i];
                fmFiles[i] = fmFiles[j];
                fmFiles[j] = temp;
                continue;
            }
            if (fmFiles[i] > fmFiles[j]) {
                String temp = fmFiles[i];
                fmFiles[i] = fmFiles[j];
                fmFiles[j] = temp;
            }
        }
    }
}

String getCleanFilename(String displayName) {
    int spaceIdx = displayName.indexOf(' ');
    if (spaceIdx > 0) {
        String name = displayName.substring(0, spaceIdx);
        if (name.endsWith("/")) name = name.substring(0, name.length() - 1);
        return name;
    }
    return displayName;
}

// =================================================
// DRAW FILE MANAGER OLED
// =================================================

void drawFileManagerOLED() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    
    // Header
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawXBMP(2, 1, 10, 10, icon_settings);
    u8g2.drawStr(15, 9, "File Manager");
    u8g2.setDrawColor(1);
    u8g2.drawLine(0, 12, 128, 12);
    
    // Path display (truncated)
    String pathDisplay = fmCurrentPath;
    if (pathDisplay.length() > 20) {
        pathDisplay = "..." + pathDisplay.substring(pathDisplay.length() - 17);
    }
    u8g2.setCursor(2, 20);
    u8g2.print("📂 ");
    u8g2.print(pathDisplay.c_str());
    
    // Show file count
    char countStr[20];
    sprintf(countStr, "%d files", fmFileCount);
    u8g2.setCursor(90, 20);
    u8g2.print(countStr);
    
    // Draw file list with scrolling
    int visibleItems = 4;
    int startY = 24;
    int lineHeight = 9;
    
    // Adjust scroll
    if (fmFileIndex < fmScrollOffset) fmScrollOffset = fmFileIndex;
    if (fmFileIndex >= fmScrollOffset + visibleItems) {
        fmScrollOffset = fmFileIndex - visibleItems + 1;
    }
    
    for (int i = 0; i < visibleItems && (fmScrollOffset + i) < fmFileCount; i++) {
        int idx = fmScrollOffset + i;
        int y = startY + i * lineHeight;
        
        String displayName = fmFiles[idx];
        bool isDir = displayName.endsWith("/");
        
        // Truncate long names
        if (displayName.length() > 20) {
            displayName = displayName.substring(0, 18) + "..";
        }
        
        if (idx == fmFileIndex) {
            u8g2.setDrawColor(0);
            u8g2.drawBox(0, y - 1, 128, lineHeight);
            u8g2.setDrawColor(1);
            u8g2.setCursor(3, y + 1);
            u8g2.print(">");
            u8g2.setCursor(12, y + 1);
        } else {
            u8g2.setDrawColor(1);
            u8g2.setCursor(3, y + 1);
            u8g2.print(" ");
            u8g2.setCursor(12, y + 1);
        }
        
        // Show folder icon for directories
        if (isDir) {
            u8g2.print("📁 ");
        } else {
            u8g2.print("📄 ");
        }
        u8g2.print(displayName.c_str());
    }
    
    // Scroll indicators
    if (fmScrollOffset > 0) {
        u8g2.setCursor(120, 22);
        u8g2.print("^");
    }
    if (fmScrollOffset + visibleItems < fmFileCount) {
        u8g2.setCursor(120, 62);
        u8g2.print("v");
    }
    
    // Bottom status bar
    u8g2.setCursor(0, 63);
    
    // Show storage info
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    char storageStr[30];
    sprintf(storageStr, "💾 %d/%dKB", used / 1024, total / 1024);
    u8g2.print(storageStr);
    
    // Show AP status
    if (fmAPStarted) {
        u8g2.setCursor(90, 63);
        u8g2.print("📡");
    }
    
    // Show navigation hints (4 buttons: UP, DOWN, SELECT, BACK)
    u8g2.setCursor(100, 63);
    u8g2.print("SEL");
    
    u8g2.sendBuffer();
}

// =================================================
// WEB INTERFACE - HTML
// =================================================
String getFileManagerHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>📁 File Manager</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0a0a0a;color:#e0e0e0;padding:20px}
.container{max-width:800px;margin:0 auto;background:#1a1a1a;border-radius:12px;padding:20px;border:1px solid #333}
h1{color:#00ff41;font-size:24px;margin-bottom:10px}
.subtitle{color:#888;font-size:14px;margin-bottom:20px}
.path-bar{background:#222;padding:12px 16px;border-radius:8px;margin-bottom:15px;display:flex;align-items:center;gap:10px;border:1px solid #333}
.path-bar .path{color:#00ff41;font-family:monospace;font-size:14px;flex:1;word-break:break-all}
.path-bar button{background:#00ff41;color:#000;border:none;padding:6px 12px;border-radius:4px;cursor:pointer;font-weight:bold}
.stats{display:flex;gap:15px;margin-bottom:15px;font-size:13px;color:#888;flex-wrap:wrap}
.stats span{background:#222;padding:6px 12px;border-radius:6px}
.file-list{background:#111;border-radius:8px;overflow:hidden}
.file-item{display:flex;align-items:center;padding:10px 14px;border-bottom:1px solid #222;cursor:pointer}
.file-item:hover{background:#1a2a1a}
.file-item .icon{font-size:18px;margin-right:12px;width:30px}
.file-item .name{flex:1;font-size:14px}
.file-item .size{color:#666;font-size:12px;margin-right:15px;min-width:60px}
.file-item .actions{display:flex;gap:6px}
.file-item .actions button{background:transparent;border:none;color:#666;cursor:pointer;padding:4px 8px;border-radius:4px;font-size:14px}
.file-item .actions button:hover{background:#333;color:#fff}
.file-item .actions .download:hover{color:#00ff41}
.file-item .actions .delete:hover{color:#ff4444}
.upload-area{border:2px dashed #333;border-radius:8px;padding:30px;text-align:center;margin-top:15px}
.upload-area:hover{border-color:#00ff41}
.upload-area input[type="file"]{display:none}
.upload-area label{color:#888;cursor:pointer;font-size:14px}
.upload-area label span{color:#00ff41}
.upload-progress{display:none;margin-top:10px;background:#222;border-radius:4px;height:20px;overflow:hidden}
.upload-progress .bar{background:#00ff41;height:100%;width:0%;transition:width 0.3s}
.status{margin-top:10px;padding:10px;border-radius:4px;background:#222;font-size:13px;display:none}
.status.success{display:block;color:#00ff41}
.status.error{display:block;color:#ff4444}
</style>
</head>
<body>
<div class="container">
    <h1>📁 File Manager</h1>
    <div class="subtitle">SPIFFS File System</div>
    <div class="path-bar">
        <span style="color:#888;">📂</span>
        <span class="path" id="currentPath">/</span>
        <button onclick="refreshFiles()">🔄</button>
    </div>
    <div class="stats">
        <span id="fileCount">0 files</span>
        <span id="storageInfo">0/0 KB</span>
    </div>
    <div class="file-list" id="fileList"><div style="padding:20px;text-align:center;color:#666;">Loading...</div></div>
    <div class="upload-area" id="uploadArea">
        <form id="uploadForm" enctype="multipart/form-data">
            <label for="fileInput">📤 <span>Click to upload</span> or drag & drop files<br><small style="color:#666;">Max 2MB</small></label>
            <input type="file" id="fileInput" name="file">
        </form>
        <div class="upload-progress" id="uploadProgress"><div class="bar" id="uploadBar"></div></div>
    </div>
    <div class="status" id="status"></div>
</div>
<script>
let currentPath = '/';
function refreshFiles() {
    fetch('/api/files?path=' + encodeURIComponent(currentPath))
        .then(r => r.json()).then(data => { renderFiles(data); })
        .catch(() => showStatus('Error loading files', 'error'));
}
function renderFiles(data) {
    const list = document.getElementById('fileList');
    document.getElementById('fileCount').textContent = data.count + ' files';
    document.getElementById('storageInfo').textContent = data.used + '/' + data.total + ' KB';
    document.getElementById('currentPath').textContent = data.path || '/';
    currentPath = data.path || '/';
    if (data.files.length === 0) {
        list.innerHTML = '<div style="padding:20px;text-align:center;color:#666;">📂 Empty directory</div>';
        return;
    }
    let html = '';
    data.files.forEach((file, index) => {
        const isDir = file.name.endsWith('/');
        const icon = isDir ? '📁' : '📄';
        const displayName = isDir ? file.name.slice(0, -1) : file.name;
        const downloadUrl = isDir ? '' : '/download?path=' + encodeURIComponent(currentPath + file.name);
        html += `<div class="file-item" onclick="${isDir ? 'enterDir(\'' + file.name + '\')' : 'viewFile(\'' + file.name + '\')'}">
            <span class="icon">${icon}</span>
            <span class="name">${displayName}</span>
            <span class="size">${isDir ? '📂' : file.size}</span>
            <span class="actions">
                ${isDir ? '' : '<button class="download" onclick="event.stopPropagation(); window.location.href=\'' + downloadUrl + '\'">⬇️</button>'}
                <button class="delete" onclick="event.stopPropagation(); deleteFile(' + (isDir ? 'null' : "'" + file.name + "'") + ')">🗑️</button>
            </span>
        </div>`;
    });
    list.innerHTML = html;
}
function enterDir(name) {
    currentPath = currentPath.endsWith('/') ? currentPath : currentPath + '/';
    if (currentPath === '/') currentPath = '/' + name;
    else currentPath = currentPath + name;
    if (!currentPath.endsWith('/')) currentPath += '/';
    refreshFiles();
}
function viewFile(name) {
    window.open('/view?path=' + encodeURIComponent(currentPath + name), '_blank');
}
function deleteFile(name) {
    if (!name) return;
    if (!confirm('Delete ' + name + '?')) return;
    fetch('/api/delete', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({path: currentPath + name}) })
    .then(r => r.json()).then(data => { if (data.success) { showStatus('✅ ' + data.message, 'success'); refreshFiles(); } else { showStatus('❌ ' + data.message, 'error'); } });
}
function showStatus(msg, type) {
    const el = document.getElementById('status');
    el.textContent = msg; el.className = 'status ' + type;
    setTimeout(() => { el.className = 'status'; }, 5000);
}
document.getElementById('fileInput').addEventListener('change', function() {
    if (this.files.length === 0) return;
    const file = this.files[0];
    if (file.size > 2 * 1024 * 1024) { showStatus('❌ File too large (max 2MB)', 'error'); return; }
    const formData = new FormData(); formData.append('file', file); formData.append('path', currentPath);
    const progress = document.getElementById('uploadProgress'); const bar = document.getElementById('uploadBar');
    progress.style.display = 'block';
    fetch('/api/upload', { method: 'POST', body: formData })
    .then(r => r.json()).then(data => {
        bar.style.width = '100%';
        setTimeout(() => { progress.style.display = 'none'; bar.style.width = '0%'; }, 500);
        if (data.success) { showStatus('✅ ' + data.message, 'success'); refreshFiles(); }
        else { showStatus('❌ ' + data.message, 'error'); }
    }).catch(() => { progress.style.display = 'none'; showStatus('❌ Upload failed', 'error'); });
    this.value = '';
});
refreshFiles();
</script>
</body>
</html>
)rawliteral";
    return html;
}

// =================================================
// WEB ROUTES
// =================================================
void setupFileManagerWeb() {
    Serial.println("🌐 Setting up File Manager Web Server...");
    
    // Check if already in AP mode
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        Serial.println("📡 Using existing AP mode");
        fmAPStarted = true;
        
        // Setup routes on main web server instead
        webServer.on("/fm", []() {
            webServer.send(200, "text/html", getFileManagerHTML());
        });
        
        webServer.on("/api/files", []() {
            String path = webServer.arg("path");
            if (path.isEmpty()) path = "/";
            
            StaticJsonDocument<2048> doc;
            JsonArray files = doc.createNestedArray("files");
            
            File root = SPIFFS.open(path);
            if (root && root.isDirectory()) {
                File file = root.openNextFile();
                while (file) {
                    JsonObject obj = files.createNestedObject();
                    String name = file.name();
                    if (name.startsWith("/")) name = name.substring(1);
                    if (file.isDirectory()) name += "/";
                    obj["name"] = name;
                    obj["size"] = file.isDirectory() ? "-" : String(file.size());
                    obj["isDir"] = file.isDirectory();
                    file.close();
                    file = root.openNextFile();
                }
                root.close();
            }
            
            doc["path"] = path;
            doc["count"] = files.size();
            doc["total"] = SPIFFS.totalBytes() / 1024;
            doc["used"] = SPIFFS.usedBytes() / 1024;
            
            String response;
            serializeJson(doc, response);
            webServer.send(200, "application/json", response);
        });
        
        webServer.on("/view", []() {
            String path = webServer.arg("path");
            if (path.isEmpty()) { webServer.send(400, "text/plain", "No path"); return; }
            
            File file = SPIFFS.open(path, "r");
            if (!file) { webServer.send(404, "text/plain", "File not found"); return; }
            
            String contentType = "text/plain";
            if (path.endsWith(".json")) contentType = "application/json";
            else if (path.endsWith(".html")) contentType = "text/html";
            else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
            else if (path.endsWith(".png")) contentType = "image/png";
            else if (path.endsWith(".pdf")) contentType = "application/pdf";
            
            webServer.streamFile(file, contentType);
            file.close();
        });
        
        webServer.on("/download", []() {
            String path = webServer.arg("path");
            if (path.isEmpty()) { webServer.send(400, "text/plain", "No path"); return; }
            
            File file = SPIFFS.open(path, "r");
            if (!file) { webServer.send(404, "text/plain", "File not found"); return; }
            
            String filename = path;
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
            if (filename.isEmpty()) filename = "file.bin";
            
            webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
            webServer.sendHeader("Cache-Control", "no-cache");
            webServer.streamFile(file, "application/octet-stream");
            file.close();
        });
        
        webServer.on("/api/delete", HTTP_POST, []() {
            StaticJsonDocument<256> doc;
            deserializeJson(doc, webServer.arg("plain"));
            String path = doc["path"] | "";
            
            if (path.isEmpty()) {
                webServer.send(400, "application/json", "{\"success\":false,\"message\":\"No path\"}");
                return;
            }
            
            if (SPIFFS.remove(path)) {
                webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Deleted\"}");
            } else {
                webServer.send(500, "application/json", "{\"success\":false,\"message\":\"Delete failed\"}");
            }
        });
        
        webServer.on("/api/upload", HTTP_POST, []() {
            webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Uploaded\"}");
        }, []() {
            HTTPUpload& upload = webServer.upload();
            static File uploadFile;
            static String uploadPath;
            
            switch (upload.status) {
                case UPLOAD_FILE_START: {
                    String path = webServer.arg("path");
                    if (path.isEmpty()) path = "/";
                    if (!path.endsWith("/")) path += "/";
                    uploadPath = path + upload.filename;
                    Serial.printf("📤 Upload: %s\n", uploadPath.c_str());
                    uploadFile = SPIFFS.open(uploadPath, "w");
                    if (!uploadFile) Serial.println("❌ Failed to create file");
                    break;
                }
                case UPLOAD_FILE_WRITE: {
                    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
                    break;
                }
                case UPLOAD_FILE_END: {
                    if (uploadFile) {
                        uploadFile.close();
                        Serial.printf("✅ Uploaded: %s (%d bytes)\n", uploadPath.c_str(), upload.totalSize);
                    }
                    break;
                }
            }
        });
        
        fmWebEnabled = true;
        Serial.println("✅ File Manager routes added to main web server");
        return;
    }
    
    // Stop existing AP if running
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        WiFi.softAPdisconnect(true);
        delay(500);
    }
    
    // Start AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-FM", "12345678");
    delay(500);
    fmAPStarted = true;
    
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("🌐 AP Started: http://%s/\n", ip.toString().c_str());
    Serial.println("🔑 Password: 12345678");
    
    // Stop main web server
    webServer.stop();
    delay(100);
    
    // Setup routes on fmWebServer
    fmWebServer.on("/", []() {
        fmWebServer.send(200, "text/html", getFileManagerHTML());
    });
    
    fmWebServer.on("/fm", []() {
        fmWebServer.send(200, "text/html", getFileManagerHTML());
    });
    
    fmWebServer.on("/api/files", []() {
        String path = fmWebServer.arg("path");
        if (path.isEmpty()) path = "/";
        
        StaticJsonDocument<2048> doc;
        JsonArray files = doc.createNestedArray("files");
        
        File root = SPIFFS.open(path);
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                JsonObject obj = files.createNestedObject();
                String name = file.name();
                if (name.startsWith("/")) name = name.substring(1);
                if (file.isDirectory()) name += "/";
                obj["name"] = name;
                obj["size"] = file.isDirectory() ? "-" : String(file.size());
                obj["isDir"] = file.isDirectory();
                file.close();
                file = root.openNextFile();
            }
            root.close();
        }
        
        doc["path"] = path;
        doc["count"] = files.size();
        doc["total"] = SPIFFS.totalBytes() / 1024;
        doc["used"] = SPIFFS.usedBytes() / 1024;
        
        String response;
        serializeJson(doc, response);
        fmWebServer.send(200, "application/json", response);
    });
    
    fmWebServer.on("/view", []() {
        String path = fmWebServer.arg("path");
        if (path.isEmpty()) { fmWebServer.send(400, "text/plain", "No path"); return; }
        
        File file = SPIFFS.open(path, "r");
        if (!file) { fmWebServer.send(404, "text/plain", "File not found"); return; }
        
        String contentType = "text/plain";
        if (path.endsWith(".json")) contentType = "application/json";
        else if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
        else if (path.endsWith(".png")) contentType = "image/png";
        else if (path.endsWith(".pdf")) contentType = "application/pdf";
        
        fmWebServer.streamFile(file, contentType);
        file.close();
    });
    
    fmWebServer.on("/download", []() {
        String path = fmWebServer.arg("path");
        if (path.isEmpty()) { fmWebServer.send(400, "text/plain", "No path"); return; }
        
        File file = SPIFFS.open(path, "r");
        if (!file) { fmWebServer.send(404, "text/plain", "File not found"); return; }
        
        String filename = path;
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
        if (filename.isEmpty()) filename = "file.bin";
        
        fmWebServer.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        fmWebServer.sendHeader("Cache-Control", "no-cache");
        fmWebServer.streamFile(file, "application/octet-stream");
        file.close();
    });
    
    fmWebServer.on("/api/delete", HTTP_POST, []() {
        StaticJsonDocument<256> doc;
        deserializeJson(doc, fmWebServer.arg("plain"));
        String path = doc["path"] | "";
        
        if (path.isEmpty()) {
            fmWebServer.send(400, "application/json", "{\"success\":false,\"message\":\"No path\"}");
            return;
        }
        
        if (SPIFFS.remove(path)) {
            fmWebServer.send(200, "application/json", "{\"success\":true,\"message\":\"Deleted\"}");
        } else {
            fmWebServer.send(500, "application/json", "{\"success\":false,\"message\":\"Delete failed\"}");
        }
    });
    
    fmWebServer.on("/api/upload", HTTP_POST, []() {
        fmWebServer.send(200, "application/json", "{\"success\":true,\"message\":\"Uploaded\"}");
    }, []() {
        HTTPUpload& upload = fmWebServer.upload();
        static File uploadFile;
        static String uploadPath;
        
        switch (upload.status) {
            case UPLOAD_FILE_START: {
                String path = fmWebServer.arg("path");
                if (path.isEmpty()) path = "/";
                if (!path.endsWith("/")) path += "/";
                uploadPath = path + upload.filename;
                Serial.printf("📤 Upload: %s\n", uploadPath.c_str());
                uploadFile = SPIFFS.open(uploadPath, "w");
                if (!uploadFile) Serial.println("❌ Failed to create file");
                break;
            }
            case UPLOAD_FILE_WRITE: {
                if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
                break;
            }
            case UPLOAD_FILE_END: {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.printf("✅ Uploaded: %s (%d bytes)\n", uploadPath.c_str(), upload.totalSize);
                }
                break;
            }
        }
    });
    
    fmWebServer.onNotFound([]() {
        fmWebServer.sendHeader("Location", "/fm", true);
        fmWebServer.send(302, "text/plain", "");
    });
    
    fmWebServer.begin();
    fmWebEnabled = true;
    Serial.println("✅ File Manager Web Server Started");
    Serial.println("   📱 URL: http://" + WiFi.softAPIP().toString() + "/fm");
}

// =================================================
// SETUP
// =================================================
void fileManagerSetup() {
    fmMenuIndex = 0;
    fmFileIndex = 0;
    fmScrollOffset = 0;
    fmFileCount = 0;
    fmCurrentPath = "/";
    fmWebEnabled = false;
    fmAPStarted = false;
    
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS mount failed");
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        CenteredStr(25, "❌ SPIFFS Error!");
        CenteredStr(40, "Mount failed");
        u8g2.sendBuffer();
        delay(2000);
        return;
    }
    
    listSpiFFS("/");
    
    // Setup web interface
    setupFileManagerWeb();
    
    Serial.println("\n📁 FILE MANAGER READY");
    Serial.println("================================");
    if (fmWebEnabled) {
        Serial.printf("🌐 Web URL: http://%s/fm\n", WiFi.softAPIP().toString().c_str());
        Serial.println("📱 Connect to WiFi: ESP32-FM");
        Serial.println("🔑 Password: 12345678");
    }
    
    // Show setup screen
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_settings);
    CenteredStr(25, "File Manager");
    CenteredStr(40, "SEL: View/Enter");
    CenteredStr(55, "SEL+DN: Delete");
    
    if (fmAPStarted) {
        u8g2.setCursor(0, 63);
        u8g2.print("📡 AP: ESP32-FM");
    }
    
    u8g2.sendBuffer();
    delay(1000);
}

// =================================================
// LOOP - 4 BUTTONS ONLY (NO LEFT/RIGHT)
// =================================================
void fileManagerLoop() {
    // Handle web server
    if (fmWebEnabled) {
        if (fmAPStarted && WiFi.getMode() == WIFI_AP) {
            fmWebServer.handleClient();
        } else {
            // Using main web server - handled externally
        }
    }
    
    drawFileManagerOLED();
    
    // Button navigation - UP
    if (isButtonPressed(BTN_UP)) {
        if (fmFileIndex > 0) {
            fmFileIndex--;
            if (fmFileIndex < fmScrollOffset) fmScrollOffset = fmFileIndex;
        } else {
            fmFileIndex = fmFileCount - 1;
            fmScrollOffset = max(0, fmFileCount - 4);
        }
        drawFileManagerOLED();
    }
    
    // Button navigation - DOWN
    if (isButtonPressed(BTN_DOWN)) {
        if (fmFileIndex < fmFileCount - 1) {
            fmFileIndex++;
            if (fmFileIndex >= fmScrollOffset + 4) {
                fmScrollOffset = fmFileIndex - 3;
            }
        } else {
            fmFileIndex = 0;
            fmScrollOffset = 0;
        }
        drawFileManagerOLED();
    }
    
    // Button navigation - SELECT (View/Enter)
    if (isButtonPressed(BTN_SELECT)) {
        if (fmFileIndex < fmFileCount && fmFileCount > 0) {
            String displayName = fmFiles[fmFileIndex];
            bool isDir = displayName.endsWith("/");
            String cleanName = getCleanFilename(displayName);
            
            if (isDir) {
                // Enter directory
                String newPath = fmCurrentPath;
                if (!newPath.endsWith("/")) newPath += "/";
                newPath += cleanName;
                if (!newPath.endsWith("/")) newPath += "/";
                fmCurrentPath = newPath;
                listSpiFFS(fmCurrentPath);
                fmFileIndex = 0;
                fmScrollOffset = 0;
                drawFileManagerOLED();
            } else {
                // View file info
                String fullPath = fmCurrentPath;
                if (!fullPath.endsWith("/")) fullPath += "/";
                fullPath += cleanName;
                
                File file = SPIFFS.open(fullPath, "r");
                if (file) {
                    u8g2.clearBuffer();
                    u8g2.setFont(u8g2_font_6x10_tr);
                    CenteredStr(10, "📄 File Info");
                    
                    char buf[30];
                    sprintf(buf, "%s", cleanName.c_str());
                    CenteredStr(22, buf);
                    
                    sprintf(buf, "Size: %d bytes", file.size());
                    CenteredStr(34, buf);
                    
                    if (file.size() < 200 && (cleanName.endsWith(".txt") || cleanName.endsWith(".json"))) {
                        String content = file.readString();
                        if (content.length() > 40) content = content.substring(0, 37) + "...";
                        CenteredStr(46, content.c_str());
                    }
                    
                    u8g2.sendBuffer();
                    file.close();
                    delay(1500);
                    drawFileManagerOLED();
                }
            }
        }
    }
    
    // ✅ DELETE: SELECT + DOWN combination (replaces BTN_RIGHT)
    if (isButtonPressed(BTN_SELECT) && isButtonPressed(BTN_DOWN)) {
        if (fmFileIndex < fmFileCount && fmFileCount > 0) {
            String displayName = fmFiles[fmFileIndex];
            bool isDir = displayName.endsWith("/");
            String cleanName = getCleanFilename(displayName);
            
            if (isDir) {
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_6x10_tr);
                CenteredStr(30, "❌ Can't delete");
                CenteredStr(42, "directories");
                u8g2.sendBuffer();
                delay(1000);
                drawFileManagerOLED();
                return;
            }
            
            // Confirm delete
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tr);
            CenteredStr(20, "🗑️ Delete file?");
            CenteredStr(35, cleanName.c_str());
            CenteredStr(50, "SEL: Yes  BACK: No");
            u8g2.sendBuffer();
            
            bool waiting = true;
            unsigned long startTime = millis();
            while (waiting) {
                if (millis() - startTime > 5000) { waiting = false; break; }
                if (isButtonPressed(BTN_SELECT)) {
                    String fullPath = fmCurrentPath;
                    if (!fullPath.endsWith("/")) fullPath += "/";
                    fullPath += cleanName;
                    if (SPIFFS.remove(fullPath)) {
                        Serial.printf("🗑️ Deleted: %s\n", fullPath.c_str());
                        listSpiFFS(fmCurrentPath);
                        if (fmFileIndex >= fmFileCount) fmFileIndex = max(0, fmFileCount - 1);
                        drawFileManagerOLED();
                    } else {
                        u8g2.clearBuffer(); 
                        CenteredStr(30, "❌ Delete failed"); 
                        u8g2.sendBuffer(); 
                        delay(1000);
                        drawFileManagerOLED();
                    }
                    waiting = false;
                }
                if (isButtonPressed(BTN_BACK)) waiting = false;
                delay(20);
            }
        }
    }
    
    // BACK - Go up or exit
    if (isButtonPressed(BTN_BACK)) {
        if (fmCurrentPath != "/" && fmCurrentPath != "") {
            // Go up one directory
            String newPath = fmCurrentPath;
            if (newPath.endsWith("/")) newPath = newPath.substring(0, newPath.length() - 1);
            int lastSlash = newPath.lastIndexOf('/');
            if (lastSlash >= 0) {
                newPath = newPath.substring(0, lastSlash + 1);
                if (newPath == "") newPath = "/";
                fmCurrentPath = newPath;
                listSpiFFS(fmCurrentPath);
                fmFileIndex = 0;
                fmScrollOffset = 0;
                drawFileManagerOLED();
            }
        } else {
            // Exit module
            if (fmWebEnabled && fmAPStarted) {
                fmWebServer.stop();
                fmWebEnabled = false;
            }
            if (fmAPStarted) {
                WiFi.softAPdisconnect(true);
                fmAPStarted = false;
                WiFi.mode(WIFI_OFF);
                delay(100);
            }
            exitModule();
            return;
        }
    }
    
    delay(20);
}

// =================================================
// CLEANUP FUNCTION
// =================================================
void fileManagerCleanup() {
    Serial.println("🧹 File Manager cleanup...");
    unsigned long startTime = millis();
    
    // 1. Stop web server if running
    if (fmWebEnabled) {
        Serial.println("  ⏹️ Stopping web server...");
        if (fmAPStarted) {
            fmWebServer.stop();
            Serial.println("  ✅ Web server stopped");
        }
        fmWebEnabled = false;
    }
    
    // 2. Disconnect AP if started
    if (fmAPStarted) {
        Serial.println("  ⏹️ Disconnecting AP...");
        WiFi.softAPdisconnect(true);
        fmAPStarted = false;
        delay(100);
        Serial.println("  ✅ AP disconnected");
    }
    
    // 3. Close any open files
    if (SPIFFS.begin(true)) {
        File root = SPIFFS.open("/");
        if (root) {
            File file = root.openNextFile();
            while (file) {
                file.close();
                file = root.openNextFile();
            }
            root.close();
        }
        SPIFFS.end();
        Serial.println("  ✅ Files closed");
    }
    
    // 4. Reset state
    fmMenuIndex = 0;
    fmFileIndex = 0;
    fmScrollOffset = 0;
    fmFileCount = 0;
    fmCurrentPath = "/";
    
    // 5. Force WiFi OFF
    WiFi.mode(WIFI_OFF);
    delay(100);
    Serial.println("  ✅ WiFi reset");
    
    Serial.printf("✅ File Manager cleanup complete (%lu ms)\n", millis() - startTime);
}