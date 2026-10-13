#include "chat.h"
#include "config.h"
#include "utils.h"
#include "icons.h"
#include "menu.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include "pins.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern WebServer webServer;
extern WebSocketsServer webSocket;
extern DNSServer dnsServer;

ChatMessage chatMessages[MAX_MESSAGES];
ChatClient chatClients[MAX_CLIENTS];
PrivateMessage privateMessages[MAX_PRIVATE_CHATS];
int messageCount = 0;
int clientCount = 0;
int privateMsgCount = 0;
String chatSSID = "Chat";
bool chatActive = false;
String adminUsername = "Admin";
ChatMode currentChatMode = CHAT_MODE_GROUP;

bool inSSIDSelectMenu = false;
int ssidSelectIndex = 0;
String availableSSIDs[20];
int availableSSIDCount = 0;
bool inChatMenu = false;
int chatMenuIndex = 0;

String getTimeStr() {
    unsigned long now = millis() / 1000;
    int hours = (now % 86400) / 3600;
    int minutes = (now % 3600) / 60;
    int seconds = now % 60;
    char timeStr[9];
    snprintf(timeStr, 9, "%02d:%02d:%02d", hours, minutes, seconds);
    return String(timeStr);
}

int findAdminClient() {
    for(int i = 0; i < clientCount; i++) {
        if(chatClients[i].active && chatClients[i].isAdmin) {
            return i;
        }
    }
    return -1;
}

int findClientByUsername(String username) {
    for(int i = 0; i < clientCount; i++) {
        if(chatClients[i].active && String(chatClients[i].username) == username) {
            return i;
        }
    }
    return -1;
}

void updateUserList() {
    String userList = "users:";
    bool first = true;
    for(int i = 0; i < clientCount; i++) {
        if(chatClients[i].active) {
            if(!first) userList += ",";
            first = false;
            userList += String(chatClients[i].username);
            if(chatClients[i].isAdmin) userList += "(Admin)";
        }
    }
    webSocket.broadcastTXT(userList);
}

void sendPrivateMessage(String from, String to, String message) {
    String timeStr = getTimeStr();
    
    if(privateMsgCount < MAX_PRIVATE_CHATS) {
        strncpy(privateMessages[privateMsgCount].from, from.c_str(), MAX_USERNAME - 1);
        strncpy(privateMessages[privateMsgCount].to, to.c_str(), MAX_USERNAME - 1);
        strncpy(privateMessages[privateMsgCount].message, message.c_str(), MAX_MESSAGE - 1);
        strncpy(privateMessages[privateMsgCount].time, timeStr.c_str(), 9);
        privateMessages[privateMsgCount].read = false;
        privateMsgCount++;
    }
    
    int targetIdx = findClientByUsername(to);
    if(targetIdx != -1) {
        String privateMsg = "private:" + from + "|" + message + "|" + timeStr;
        webSocket.sendTXT(chatClients[targetIdx].wsNum, privateMsg);
    }
    
    int senderIdx = findClientByUsername(from);
    if(senderIdx != -1) {
        String confirm = "private_sent:To " + to + "|" + message + "|" + timeStr;
        webSocket.sendTXT(chatClients[senderIdx].wsNum, confirm);
    }
    
    if(from != adminUsername) {
        int adminIdx = findAdminClient();
        if(adminIdx != -1) {
            String adminMsg = "private_admin:" + from + "|" + message + "|" + timeStr;
            webSocket.sendTXT(chatClients[adminIdx].wsNum, adminMsg);
        }
    }
}

void broadcastGroupMessage(String username, String message) {
    String timeStr = getTimeStr();
    
    if(messageCount < MAX_MESSAGES) {
        strncpy(chatMessages[messageCount].username, username.c_str(), MAX_USERNAME - 1);
        chatMessages[messageCount].username[MAX_USERNAME - 1] = '\0';
        strncpy(chatMessages[messageCount].message, message.c_str(), MAX_MESSAGE - 1);
        chatMessages[messageCount].message[MAX_MESSAGE - 1] = '\0';
        strncpy(chatMessages[messageCount].time, timeStr.c_str(), 9);
        chatMessages[messageCount].used = true;
        messageCount++;
    }
    
    webSocket.broadcastTXT("msg:" + String(messageCount - 1) + "|" + username + "|" + message + "|" + timeStr);
}

void handleCommand(uint8_t wsNum, String username, String command) {
    String response = "system:";
    
    if(command == "/help") {
        if(username == adminUsername) {
            response += "📋 Admin: /users, /clear, /help, /mode, /private @user msg, /group, /kick @user";
        } else {
            response += "📋 Commands: /users, /clear, /help, /private @user msg, /admin (msg admin)";
        }
    }
    else if(command == "/users") {
        response += "👥 Users: ";
        int activeCount = 0;
        for(int i = 0; i < clientCount; i++) {
            if(chatClients[i].active) {
                if(activeCount > 0) response += ", ";
                response += chatClients[i].username;
                if(chatClients[i].isAdmin) response += " 👑";
                activeCount++;
            }
        }
        if(activeCount == 0) response += "No users";
    }
    else if(command == "/clear") {
        webSocket.sendTXT(wsNum, "clear");
        response = "Chat cleared";
    }
    else if(command == "/mode") {
        if(username == adminUsername) {
            response = "Current mode: ";
            response += (currentChatMode == CHAT_MODE_GROUP) ? "Group" : "Private";
            response += " (Use /group or /private to switch)";
        } else {
            response = "Only admin can change mode";
        }
    }
    else if(command == "/group") {
        if(username == adminUsername) {
            currentChatMode = CHAT_MODE_GROUP;
            response = "✅ Switched to GROUP chat mode";
            webSocket.broadcastTXT("msg:system|" + response + "|" + getTimeStr());
        } else {
            response = "Only admin can change mode";
        }
    }
    else if(command == "/private") {
        if(username == adminUsername) {
            currentChatMode = CHAT_MODE_PRIVATE;
            response = "✅ Switched to PRIVATE chat mode";
            webSocket.broadcastTXT("msg:system|" + response + "|" + getTimeStr());
        } else {
            response = "Only admin can change mode";
        }
    }
    else if(command.startsWith("/private ")) {
        String rest = command.substring(9);
        int spaceIdx = rest.indexOf(' ');
        if(spaceIdx != -1) {
            String target = rest.substring(0, spaceIdx);
            String msg = rest.substring(spaceIdx + 1);
            if(target.startsWith("@")) target = target.substring(1);
            sendPrivateMessage(username, target, msg);
            response = "Private msg sent to " + target;
        } else {
            response = "Usage: /private @username message";
        }
    }
    else if(command.startsWith("/kick ")) {
        if(username == adminUsername) {
            String target = command.substring(6);
            if(target.startsWith("@")) target = target.substring(1);
            int idx = findClientByUsername(target);
            if(idx != -1 && !chatClients[idx].isAdmin) {
                webSocket.sendTXT(chatClients[idx].wsNum, "kick");
                chatClients[idx].active = false;
                String kickMsg = "👢 " + target + " was kicked by admin";
                broadcastGroupMessage("system", kickMsg);
                updateUserList();
                response = "Kicked " + target;
            } else {
                response = "User not found or is admin";
            }
        } else {
            response = "Only admin can kick users";
        }
    }
    else if(command == "/admin") {
        if(username != adminUsername) {
            sendPrivateMessage(username, adminUsername, "📩 Admin: I need assistance!");
            response = "Message sent to admin privately!";
        } else {
            response = "You are the admin!";
        }
    }
    else if(command == "/whoami") {
        response = "You are " + username + " (IP: " + webSocket.remoteIP(wsNum).toString() + ")";
        if(username == adminUsername) response += " 👑 Admin";
    }
    else {
        response = "Unknown. Type /help";
    }
    
    webSocket.sendTXT(wsNum, "msg:system|" + response + "|" + getTimeStr());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    String msg = String((char*)payload);
    
    switch(type) {
        case WStype_DISCONNECTED: {
            for(int i = 0; i < clientCount; i++) {
                if(chatClients[i].wsNum == num && chatClients[i].active) {
                    chatClients[i].active = false;
                    String leaveMsg = "👤 " + String(chatClients[i].username) + " left";
                    broadcastGroupMessage("system", leaveMsg);
                    updateUserList();
                    break;
                }
            }
            break;
        }
        
        case WStype_CONNECTED: {
            webSocket.sendTXT(num, "connected:Welcome!");
            break;
        }
        
        case WStype_TEXT: {
            if(msg.startsWith("join:")) {
                String username = msg.substring(5);
                username.trim();
                if(username.length() == 0) username = "User" + String(random(1000, 9999));
                if(username.length() > MAX_USERNAME - 1) username = username.substring(0, MAX_USERNAME - 1);
                
                bool isAdmin = (username == adminUsername);
                bool exists = false;
                
                for(int i = 0; i < clientCount; i++) {
                    if(chatClients[i].wsNum == num) {
                        chatClients[i].active = true;
                        strncpy(chatClients[i].username, username.c_str(), MAX_USERNAME - 1);
                        chatClients[i].username[MAX_USERNAME - 1] = '\0';
                        chatClients[i].lastSeen = millis();
                        chatClients[i].isAdmin = isAdmin;
                        exists = true;
                        break;
                    }
                }
                
                if(!exists && clientCount < MAX_CLIENTS) {
                    chatClients[clientCount].wsNum = num;
                    strncpy(chatClients[clientCount].username, username.c_str(), MAX_USERNAME - 1);
                    chatClients[clientCount].username[MAX_USERNAME - 1] = '\0';
                    chatClients[clientCount].active = true;
                    chatClients[clientCount].lastSeen = millis();
                    chatClients[clientCount].isAdmin = isAdmin;
                    String ip = webSocket.remoteIP(num).toString();
                    strncpy(chatClients[clientCount].ip, ip.c_str(), 15);
                    clientCount++;
                }
                
                String welcome = "👋 Welcome " + username;
                if(isAdmin) welcome += " (Admin)";
                webSocket.sendTXT(num, "msg:system|" + welcome + "|" + getTimeStr());
                
                String joinMsg = "👤 " + username + " joined";
                broadcastGroupMessage("system", joinMsg);
                
                String history = "history:";
                bool first = true;
                int startIdx = max(0, messageCount - 50);
                for(int i = startIdx; i < messageCount; i++) {
                    if(!first) history += "||";
                    first = false;
                    history += String(i) + "|" + String(chatMessages[i].username) + "|" + 
                              String(chatMessages[i].message) + "|" + String(chatMessages[i].time);
                }
                webSocket.sendTXT(num, history);
                
                updateUserList();
            }
            
            else if(msg.startsWith("msg:")) {
                String data = msg.substring(4);
                int sep1 = data.indexOf('|');
                if(sep1 != -1) {
                    String msgId = data.substring(0, sep1);
                    String remaining = data.substring(sep1 + 1);
                    int sep2 = remaining.indexOf('|');
                    if(sep2 != -1) {
                        String username = remaining.substring(0, sep2);
                        String message = remaining.substring(sep2 + 1);
                        
                        if(message.startsWith("/")) {
                            handleCommand(num, username, message);
                            return;
                        }
                        
                        if(currentChatMode == CHAT_MODE_PRIVATE || message.startsWith("@")) {
                            String target = adminUsername;
                            if(message.startsWith("@")) {
                                int spaceIdx = message.indexOf(' ');
                                if(spaceIdx != -1) {
                                    target = message.substring(1, spaceIdx);
                                    message = message.substring(spaceIdx + 1);
                                }
                            }
                            sendPrivateMessage(username, target, message);
                            if(username != adminUsername) {
                                String confirm = "system:Private msg sent to " + target;
                                webSocket.sendTXT(num, "msg:system|" + confirm + "|" + getTimeStr());
                            }
                        } else {
                            broadcastGroupMessage(username, message);
                        }
                    }
                }
            }
            
            else if(msg.startsWith("typing:")) {
                String username = msg.substring(7);
                for(int i = 0; i < clientCount; i++) {
                    if(chatClients[i].wsNum != num && chatClients[i].active) {
                        webSocket.sendTXT(chatClients[i].wsNum, "typing:" + username);
                    }
                }
            }
            
            break;
        }
        
        default: break;
    }
}

void scanAvailableSSIDs() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    availableSSIDCount = 0;
    if(n > 0) {
        for(int i = 0; i < n && availableSSIDCount < 20; i++) {
            String ssid = WiFi.SSID(i);
            if(ssid.length() > 0 && ssid.length() <= 32) {
                availableSSIDs[availableSSIDCount++] = ssid;
            }
        }
    }
    WiFi.scanDelete();
    
    if(availableSSIDCount < 20) {
        availableSSIDs[availableSSIDCount++] = "Chat";
        availableSSIDs[availableSSIDCount++] = "Chat_Group";
        availableSSIDs[availableSSIDCount++] = "Chat_Private";
    }
}

String getChatPageHTML() {
    String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>💬 Chat</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0b141a;color:#e9edef;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;display:flex;justify-content:center;min-height:100vh;overflow:hidden}
.chat{max-width:420px;width:100%;height:100vh;display:flex;flex-direction:column;background:#0b141a;position:relative}
.header{background:#075e54;padding:10px 15px;display:flex;justify-content:space-between;align-items:center;flex-shrink:0}
.header .title{font-size:16px;font-weight:bold;color:#fff;display:flex;align-items:center;gap:6px}
.header .title .online{font-size:11px;font-weight:normal;color:#a8d5c7}
.header .menu-btn{background:none;border:none;color:#fff;font-size:18px;cursor:pointer;padding:0 6px}
.status-bar{background:#1f2c33;padding:3px 12px;font-size:10px;color:#667781;text-align:center;flex-shrink:0;display:flex;justify-content:space-between}
.messages{flex:1;overflow-y:auto;padding:8px 10px;display:flex;flex-direction:column;gap:2px;scroll-behavior:smooth}
.msg{margin:2px 0;padding:7px 10px;border-radius:8px;max-width:85%;word-wrap:break-word;animation:fadeIn 0.2s ease}
@keyframes fadeIn{from{opacity:0;transform:translateY(5px)}to{opacity:1;transform:translateY(0)}}
.sent{background:#075e54;color:#fff;align-self:flex-end;border-bottom-right-radius:2px}
.received{background:#1f2c33;color:#e9edef;align-self:flex-start;border-bottom-left-radius:2px}
.system{text-align:center;color:#667781;font-size:10px;padding:3px 8px;font-style:italic;align-self:center;background:rgba(31,44,51,0.5);border-radius:12px;margin:1px 0}
.private{background:#2a1f3d;color:#d4b8ff;align-self:flex-start;border-left:3px solid #9b59b6;border-bottom-left-radius:2px}
.private-sent{background:#1a2a3d;color:#7ec8e3;align-self:flex-end;border-right:3px solid #3498db;border-bottom-right-radius:2px}
.msg-user{font-size:9px;font-weight:bold;color:#a8d5c7;margin-bottom:1px}
.msg-time{font-size:8px;color:#667781;margin-top:2px;text-align:right}
.private .msg-user{color:#b388ff}
.private-sent .msg-user{color:#64b5f6}
.input-area{display:flex;padding:6px 8px;gap:4px;background:#1f2c33;flex-shrink:0;border-top:1px solid #2a3942}
.input-area input{flex:1;padding:8px 10px;border:none;border-radius:6px;background:#2a3942;color:#e9edef;font-size:13px;outline:none;min-width:0}
.input-area input::placeholder{color:#667781}
.input-area button{padding:8px 14px;border:none;border-radius:6px;background:#075e54;color:#fff;font-weight:bold;cursor:pointer;font-size:13px;white-space:nowrap}
.input-area button:hover{background:#0a7a6e}
.typing{color:#667781;font-size:11px;padding:2px 10px;min-height:18px;flex-shrink:0}
.user-list-popup{display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.7);z-index:100;justify-content:center;align-items:center}
.user-list-popup.show{display:flex}
.user-list-box{background:#1f2c33;border-radius:12px;padding:20px;max-width:300px;width:90%;max-height:70vh;overflow-y:auto}
.user-list-box h3{color:#e9edef;margin-bottom:12px;font-size:16px}
.user-list-box .user-item{color:#e9edef;padding:6px 0;border-bottom:1px solid #2a3942;font-size:13px;display:flex;justify-content:space-between}
.user-list-box .user-item .admin-badge{color:#f1c40f;font-size:10px}
.user-list-box .close-btn{width:100%;padding:8px;margin-top:12px;background:#075e54;color:#fff;border:none;border-radius:6px;cursor:pointer}
</style>
</head>
<body>
<div class="chat">
<div class="header">
<div class="title">💬 <span id="roomName">Chat</span> <span class="online" id="userCount">0</span></div>
<button class="menu-btn" onclick="toggleUserList()">👥</button>
</div>
<div class="status-bar"><span id="statusMsg">Connected</span><span id="timeDisplay"></span></div>
<div class="messages" id="messages"></div>
<div class="typing" id="typing"></div>
<div class="input-area">
<input id="input" placeholder="Type message... (@user for private)" autofocus>
<button onclick="send()">Send</button>
</div>
</div>

<div class="user-list-popup" id="userListPopup">
<div class="user-list-box">
<h3>👥 Online Users</h3>
<div id="userListContent"></div>
<button class="close-btn" onclick="toggleUserList()">Close</button>
</div>
</div>

<script>
var user = 'User_' + Math.random().toString(36).substr(2, 4);
var ws = new WebSocket('ws://' + location.hostname + ':81');
var messages = document.getElementById('messages');
var input = document.getElementById('input');
var typing = document.getElementById('typing');
var seen = new Set();
var userList = [];

ws.onopen = function(){ 
    ws.send('join:' + user); 
    document.getElementById('statusMsg').textContent = 'Connected';
};

ws.onclose = function(){ 
    document.getElementById('statusMsg').textContent = 'Reconnecting...';
    setTimeout(function(){ location.reload(); }, 3000);
};

ws.onmessage = function(e){
    var data = e.data;
    
    if(data.startsWith('msg:')){
        var p = data.substr(4).split('|');
        if(p.length >= 4){
            var id = p[0], sender = p[1], msg = p[2], time = p[3];
            if(!seen.has(id)){
                seen.add(id);
                if(sender === 'system'){
                    addSystemMessage(msg);
                } else {
                    var isSent = (sender === user);
                    addMessage(id, sender, msg, time, isSent ? 'sent' : 'received');
                }
                scrollToBottom();
            }
        }
    }
    else if(data.startsWith('history:')){
        var msgs = data.substr(8).split('||');
        msgs.forEach(function(m){
            if(m.length > 0){
                var p = m.split('|');
                if(p.length >= 4){
                    var id = p[0], sender = p[1], msg = p[2], time = p[3];
                    if(!seen.has(id)){
                        seen.add(id);
                        if(sender === 'system'){
                            addSystemMessage(msg);
                        } else {
                            var isSent = (sender === user);
                            addMessage(id, sender, msg, time, isSent ? 'sent' : 'received');
                        }
                    }
                }
            }
        });
        scrollToBottom();
    }
    else if(data.startsWith('private:')){
        var p = data.substr(8).split('|');
        if(p.length >= 3){
            var from = p[0], msg = p[1], time = p[2];
            addMessage('priv_' + Date.now(), '🔒 ' + from, msg, time, 'private');
            scrollToBottom();
        }
    }
    else if(data.startsWith('private_sent:')){
        var p = data.substr(13).split('|');
        if(p.length >= 3){
            var to = p[0], msg = p[1], time = p[2];
            addMessage('psent_' + Date.now(), '🔒 → ' + to, msg, time, 'private-sent');
            scrollToBottom();
        }
    }
    else if(data.startsWith('private_admin:')){
        var p = data.substr(14).split('|');
        if(p.length >= 3){
            var from = p[0], msg = p[1], time = p[2];
            addMessage('padm_' + Date.now(), '🔒 ' + from + ' (private)', msg, time, 'private');
            scrollToBottom();
            document.getElementById('statusMsg').textContent = '💬 Private from ' + from;
            setTimeout(function(){ document.getElementById('statusMsg').textContent = 'Connected'; }, 3000);
        }
    }
    else if(data.startsWith('users:')){
        var users = data.substr(6).split(',');
        userList = users;
        updateUserListDisplay();
        document.getElementById('userCount').textContent = users.length + ' online';
    }
    else if(data.startsWith('typing:')){
        var sender = data.substr(7);
        if(sender !== user){
            typing.textContent = sender + ' typing...';
            clearTimeout(window.typingTimeout);
            window.typingTimeout = setTimeout(function(){ typing.textContent = ''; }, 3000);
        }
    }
    else if(data === 'clear'){
        messages.innerHTML = '';
    }
    else if(data === 'kick'){
        alert('You have been kicked by admin!');
        ws.close();
        setTimeout(function(){ location.reload(); }, 2000);
    }
};

function addMessage(id, sender, msg, time, cls){
    var div = document.createElement('div');
    div.className = 'msg ' + cls;
    var displayName = (sender === user && !sender.startsWith('🔒')) ? 'You' : sender;
    div.innerHTML = '<div class="msg-user">' + displayName + '</div>' + 
                    escapeHtml(msg) + 
                    '<div class="msg-time">' + time + '</div>';
    messages.appendChild(div);
}

function addSystemMessage(msg){
    var div = document.createElement('div');
    div.className = 'system';
    div.textContent = msg;
    messages.appendChild(div);
}

function scrollToBottom(){
    messages.scrollTop = messages.scrollHeight;
}

function toggleUserList(){
    document.getElementById('userListPopup').classList.toggle('show');
}

function updateUserListDisplay(){
    var content = document.getElementById('userListContent');
    content.innerHTML = '';
    userList.forEach(function(u){
        var div = document.createElement('div');
        div.className = 'user-item';
        var isAdminUser = u.includes('(Admin)');
        var name = u.replace('(Admin)', '').trim();
        var star = (name === user) ? '⭐ ' : '';
        var badge = isAdminUser ? ' 👑' : '';
        div.innerHTML = '<span>' + star + name + badge + '</span>' + 
                       '<span class="admin-badge">' + (isAdminUser ? '●' : '○') + '</span>';
        content.appendChild(div);
    });
    if(userList.length === 0){
        content.innerHTML = '<div class="user-item">No users online</div>';
    }
}

function send(){
    var t = input.value.trim();
    if(t === '') return;
    var id = Date.now() + '-' + Math.random().toString(36).substr(2, 4);
    ws.send('msg:' + id + '|' + user + '|' + t);
    input.value = '';
    input.focus();
    typing.textContent = '';
}

function escapeHtml(t){ var d=document.createElement('div'); d.textContent=t; return d.innerHTML; }

input.addEventListener('input', function(){
    clearTimeout(window.sendTyping);
    window.sendTyping = setTimeout(function(){ 
        if(input.value.trim() !== '') ws.send('typing:' + user); 
    }, 300);
});

input.addEventListener('keypress', function(e){ if(e.key === 'Enter') send(); });

function updateTime(){
    var now = new Date();
    var h = String(now.getHours()).padStart(2,'0');
    var m = String(now.getMinutes()).padStart(2,'0');
    document.getElementById('timeDisplay').textContent = h + ':' + m;
}
setInterval(updateTime, 10000);
updateTime();

window.addEventListener('load', function(){ 
    setTimeout(function(){ input.focus(); }, 500);
});
</script>
</body>
</html>
)rawliteral";
    return page;
}

String getAdminPageHTML() {
    String adminPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>👑 Admin Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0b141a;color:#e9edef;font-family:'Segoe UI',sans-serif;padding:20px}
.container{max-width:900px;margin:0 auto}
h1{color:#075e54;border-bottom:2px solid #075e54;padding-bottom:10px;margin-bottom:20px}
.card{background:#1f2c33;border-radius:10px;padding:15px;margin-bottom:15px;border:1px solid #2a3942}
.card h3{color:#075e54;margin-bottom:8px}
.status-bar{display:flex;gap:15px;flex-wrap:wrap;margin-bottom:15px}
.status-item{background:#2a3942;padding:8px 15px;border-radius:6px;font-size:14px}
.status-item span{color:#a8d5c7}
table{width:100%;border-collapse:collapse;font-size:13px}
th{text-align:left;color:#a8d5c7;padding:8px 4px;border-bottom:1px solid #2a3942}
td{padding:8px 4px;border-bottom:1px solid #1f2c33}
.btn{background:#075e54;color:#fff;border:none;padding:8px 16px;border-radius:6px;cursor:pointer;margin:4px}
.btn:hover{background:#0a7a6e}
.btn-danger{background:#c0392b}
.btn-danger:hover{background:#e74c3c}
.btn-warning{background:#f39c12}
.btn-warning:hover{background:#e67e22}
.mode-indicator{display:inline-block;padding:4px 12px;border-radius:12px;font-size:12px;font-weight:bold}
.mode-group{background:#075e54;color:#fff}
.mode-private{background:#9b59b6;color:#fff}
.refresh-btn{float:right;background:#2a3942;border:none;color:#fff;padding:4px 12px;border-radius:4px;cursor:pointer}
.private-msg{background:#2a1f3d;border-left:3px solid #9b59b6;padding:8px 12px;margin:4px 0;border-radius:4px}
.private-msg .from{color:#b388ff;font-weight:bold}
.private-msg .time{color:#667781;font-size:11px;float:right}
.group-msg{background:#1a2a3d;border-left:3px solid #075e54;padding:8px 12px;margin:4px 0;border-radius:4px}
.group-msg .from{color:#7ec8e3;font-weight:bold}
.group-msg .time{color:#667781;font-size:11px;float:right}
</style>
</head>
<body>
<div class="container">
<h1>👑 Admin Dashboard</h1>

<div class="status-bar">
<div class="status-item">📡 SSID: <span id="ssid">)rawliteral";
    adminPage += chatSSID;
    adminPage += R"rawliteral(</span></div>
<div class="status-item">👥 Clients: <span id="clientCount">)rawliteral";
    adminPage += String(WiFi.softAPgetStationNum());
    adminPage += R"rawliteral(</span></div>
<div class="status-item">📋 Mode: <span id="modeDisplay">)rawliteral";
    adminPage += (currentChatMode == CHAT_MODE_GROUP) ? "Group" : "Private";
    adminPage += R"rawliteral(</span></div>
<div class="status-item">💬 Messages: <span id="msgCount">)rawliteral";
    adminPage += String(messageCount);
    adminPage += R"rawliteral(</span></div>
<div class="status-item">🔒 Private: <span id="privCount">)rawliteral";
    adminPage += String(privateMsgCount);
    adminPage += R"rawliteral(</span></div>
</div>

<div class="card">
<h3>📢 Chat Mode Control</h3>
<button class="btn" onclick="setMode('group')">📢 Group Chat</button>
<button class="btn" onclick="setMode('private')">🔒 Private Chat</button>
<button class="btn btn-danger" onclick="stopChat()">⏹️ Stop Chat</button>
<button class="btn btn-warning" onclick="clearChat()">🗑️ Clear Messages</button>
<span id="modeStatus" class="mode-indicator mode-group" style="margin-left:10px">)rawliteral";
    adminPage += (currentChatMode == CHAT_MODE_GROUP) ? "Group" : "Private";
    adminPage += R"rawliteral(</span>
</div>

<div class="card">
<h3>👥 Connected Clients <button class="refresh-btn" onclick="refreshClients()">⟳ Refresh</button></h3>
<div id="clientList"><p>Loading...</p></div>
</div>

<div class="card">
<h3>💬 Private Messages from Clients</h3>
<div id="privateMessages"><p>No private messages yet</p></div>
</div>

<div class="card">
<h3>📝 Recent Group Messages</h3>
<div id="groupMessages"><p>No messages yet</p></div>
</div>

</div>

<script>
function refreshClients() {
    fetch('/api/clients')
        .then(r => r.json())
        .then(data => {
            let html = '';
            if(data.clients && data.clients.length > 0) {
                html = '<table><tr><th>Username</th><th>IP</th><th>Status</th><th>Action</th></tr>';
                data.clients.forEach(c => {
                    let status = c.active ? '🟢 Online' : '🔴 Offline';
                    let badge = c.isAdmin ? ' 👑' : '';
                    html += `<tr>
                        <td>${c.username}${badge}</td>
                        <td>${c.ip}</td>
                        <td>${status}</td>
                        <td>${c.isAdmin ? 'Admin' : '<button class="btn btn-danger" onclick="kickClient(\''+c.username+'\')">Kick</button>'}</td>
                    </tr>`;
                });
                html += '</table>';
            } else {
                html = '<p>No clients connected</p>';
            }
            document.getElementById('clientList').innerHTML = html;
            document.getElementById('clientCount').textContent = data.count || 0;
        })
        .catch(() => {});
}

function refreshPrivateMessages() {
    fetch('/api/private')
        .then(r => r.json())
        .then(data => {
            let html = '';
            if(data.messages && data.messages.length > 0) {
                data.messages.slice(-20).forEach(m => {
                    html += `<div class="private-msg">
                        <span class="from">${m.from}</span> → ${m.to}: ${m.message}
                        <span class="time">${m.time}</span>
                    </div>`;
                });
            } else {
                html = '<p>No private messages</p>';
            }
            document.getElementById('privateMessages').innerHTML = html;
        })
        .catch(() => {});
}

function refreshGroupMessages() {
    fetch('/api/messages')
        .then(r => r.json())
        .then(data => {
            let html = '';
            if(data.messages && data.messages.length > 0) {
                data.messages.slice(-20).forEach(m => {
                    let isAdmin = m.username === 'Admin' ? '👑 ' : '';
                    html += `<div class="group-msg">
                        <span class="from">${isAdmin}${m.username}</span>: ${m.message}
                        <span class="time">${m.time}</span>
                    </div>`;
                });
            } else {
                html = '<p>No messages yet</p>';
            }
            document.getElementById('groupMessages').innerHTML = html;
        })
        .catch(() => {});
}

function setMode(mode) {
    fetch('/api/mode?mode=' + mode)
        .then(r => r.text())
        .then(() => {
            let modeText = mode === 'group' ? 'Group' : 'Private';
            document.getElementById('modeStatus').textContent = modeText;
            document.getElementById('modeStatus').className = 'mode-indicator ' + (mode === 'group' ? 'mode-group' : 'mode-private');
            document.getElementById('modeDisplay').textContent = modeText;
            refreshClients();
        });
}

function stopChat() {
    if(confirm('Stop chat server?')) {
        fetch('/api/stop')
            .then(() => { alert('Chat stopped'); location.reload(); });
    }
}

function clearChat() {
    if(confirm('Clear all messages?')) {
        fetch('/api/clear')
            .then(() => { refreshGroupMessages(); refreshPrivateMessages(); });
    }
}

function kickClient(username) {
    if(confirm('Kick ' + username + '?')) {
        fetch('/api/kick?user=' + username)
            .then(() => { refreshClients(); });
    }
}

refreshClients();
refreshPrivateMessages();
refreshGroupMessages();
setInterval(refreshClients, 5000);
setInterval(refreshPrivateMessages, 3000);
setInterval(refreshGroupMessages, 5000);
</script>
</body>
</html>
)rawliteral";
    return adminPage;
}

void startChatAP(String ssid) {
    if(chatActive) {
        webSocket.close();
        webServer.stop();
        WiFi.softAPdisconnect(true);
        delay(500);
    }
    
    chatSSID = ssid;
    if(chatSSID.length() == 0) chatSSID = "Chat";
    if(chatSSID.length() > 32) chatSSID = chatSSID.substring(0, 32);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(chatSSID.c_str());
    delay(100);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    for(int i = 0; i < MAX_CLIENTS; i++) chatClients[i].active = false;
    for(int i = 0; i < MAX_MESSAGES; i++) chatMessages[i].used = false;
    clientCount = 0;
    messageCount = 0;
    privateMsgCount = 0;
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    String chatPage = getChatPageHTML();
    String adminPage = getAdminPageHTML();
    
    webServer.on("/", [chatPage]() {
        webServer.send(200, "text/html", chatPage);
    });
    
    webServer.on("/chat", [chatPage]() {
        webServer.send(200, "text/html", chatPage);
    });
    
    webServer.on("/admin", [adminPage]() {
        webServer.send(200, "text/html", adminPage);
    });

    webServer.on("/api/clients", []() {
        String json = "{\"count\":" + String(clientCount) + ",\"clients\":[";
        bool first = true;
        for(int i = 0; i < clientCount; i++) {
            if(!first) json += ",";
            first = false;
            json += "{";
            json += "\"username\":\"" + String(chatClients[i].username) + "\",";
            json += "\"ip\":\"" + String(chatClients[i].ip) + "\",";
            json += "\"active\":" + String(chatClients[i].active ? "true" : "false") + ",";
            json += "\"isAdmin\":" + String(chatClients[i].isAdmin ? "true" : "false");
            json += "}";
        }
        json += "]}";
        webServer.send(200, "application/json", json);
    });

    webServer.on("/api/private", []() {
        String json = "{\"messages\":[";
        bool first = true;
        int start = max(0, privateMsgCount - 30);
        for(int i = start; i < privateMsgCount; i++) {
            if(!first) json += ",";
            first = false;
            json += "{";
            json += "\"from\":\"" + String(privateMessages[i].from) + "\",";
            json += "\"to\":\"" + String(privateMessages[i].to) + "\",";
            json += "\"message\":\"" + String(privateMessages[i].message) + "\",";
            json += "\"time\":\"" + String(privateMessages[i].time) + "\"";
            json += "}";
        }
        json += "]}";
        webServer.send(200, "application/json", json);
    });

    webServer.on("/api/messages", []() {
        String json = "{\"messages\":[";
        bool first = true;
        int start = max(0, messageCount - 30);
        for(int i = start; i < messageCount; i++) {
            if(!first) json += ",";
            first = false;
            json += "{";
            json += "\"username\":\"" + String(chatMessages[i].username) + "\",";
            json += "\"message\":\"" + String(chatMessages[i].message) + "\",";
            json += "\"time\":\"" + String(chatMessages[i].time) + "\"";
            json += "}";
        }
        json += "]}";
        webServer.send(200, "application/json", json);
    });

    webServer.on("/api/mode", []() {
        String mode = webServer.arg("mode");
        if(mode == "group") {
            currentChatMode = CHAT_MODE_GROUP;
            webSocket.broadcastTXT("msg:system|Admin switched to GROUP mode|" + getTimeStr());
            webServer.send(200, "text/plain", "Group mode set");
        } else if(mode == "private") {
            currentChatMode = CHAT_MODE_PRIVATE;
            webSocket.broadcastTXT("msg:system|Admin switched to PRIVATE mode|" + getTimeStr());
            webServer.send(200, "text/plain", "Private mode set");
        } else {
            webServer.send(400, "text/plain", "Invalid mode");
        }
    });

    webServer.on("/api/stop", []() {
        webServer.send(200, "text/plain", "Stopping...");
        stopChatAP();
    });

    webServer.on("/api/clear", []() {
        messageCount = 0;
        privateMsgCount = 0;
        for(int i = 0; i < MAX_MESSAGES; i++) chatMessages[i].used = false;
        webSocket.broadcastTXT("clear");
        webServer.send(200, "text/plain", "Cleared");
    });

    webServer.on("/api/kick", []() {
        String username = webServer.arg("user");
        int idx = findClientByUsername(username);
        if(idx != -1) {
            webSocket.sendTXT(chatClients[idx].wsNum, "kick");
            chatClients[idx].active = false;
            String kickMsg = "👢 " + username + " was kicked by admin";
            broadcastGroupMessage("system", kickMsg);
            updateUserList();
            webServer.send(200, "text/plain", "Kicked");
        } else {
            webServer.send(404, "text/plain", "User not found");
        }
    });

    webServer.onNotFound([]() {
        webServer.sendHeader("Location", "/", true);
        webServer.send(302, "text/plain", "");
    });

    webServer.begin();
    chatActive = true;
    
    String ipStr = WiFi.softAPIP().toString();
    showPopupMessage("✅ CHAT RUNNING!", chatSSID.c_str(), ipStr.c_str());
    delay(1500);
}

void stopChatAP() {
    if(chatActive) {
        webSocket.close();
        webServer.stop();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        chatActive = false;
        showPopupMessage("⏹️ Chat Stopped", "Server offline");
        delay(1000);
    }
}

void chatSetup() {
    chatActive = false;
    chatSSID = "Chat";
    clientCount = 0;
    messageCount = 0;
    privateMsgCount = 0;
    currentChatMode = CHAT_MODE_GROUP;
    inChatMenu = false;
    chatMenuIndex = 0;
    inSSIDSelectMenu = false;
    ssidSelectIndex = 0;
    
    for(int i = 0; i < MAX_MESSAGES; i++) chatMessages[i].used = false;
    for(int i = 0; i < MAX_CLIENTS; i++) chatClients[i].active = false;
    for(int i = 0; i < MAX_PRIVATE_CHATS; i++) privateMessages[i].read = false;
    
    clearPopup();
    showPopupMessage("💬 Chat System", "SELECT: Start AP", "SSID: Chat");
    delay(1500);
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawXBMP(56, 5, 16, 16, icon_chat);
    CenteredStr(25, "Chat System");
    CenteredStr(40, "SELECT: Start AP");
    CenteredStr(55, "SSID: Chat");
    u8g2.sendBuffer();
}

void chatLoop() {
    if (drawPopupIfActive()) {
        delay(50);
        return;
    }
    
    static int chatMenu = 0;
    const char* mainItems[] = {"Start Chat", "Stop Chat", "Select SSID", "Set Mode", "Status", "< Back"};
    int count = 6;
    
    if(inSSIDSelectMenu) {
        if(availableSSIDCount == 0) {
            scanAvailableSSIDs();
        }
        
        const char** ssidItems = new const char*[availableSSIDCount + 1];
        for(int i = 0; i < availableSSIDCount; i++) {
            ssidItems[i] = availableSSIDs[i].c_str();
        }
        ssidItems[availableSSIDCount] = "< Back>";
        
        drawScrollingMenu(ssidItems, availableSSIDCount + 1, ssidSelectIndex, 4, "Select SSID", 0);
        
        if(isButtonPressed(BTN_UP)) {
            ssidSelectIndex = (ssidSelectIndex - 1 + availableSSIDCount + 1) % (availableSSIDCount + 1);
        }
        if(isButtonPressed(BTN_DOWN)) {
            ssidSelectIndex = (ssidSelectIndex + 1) % (availableSSIDCount + 1);
        }
        if(isButtonPressed(BTN_SELECT)) {
            if(ssidSelectIndex < availableSSIDCount) {
                chatSSID = availableSSIDs[ssidSelectIndex];
                inSSIDSelectMenu = false;
                ssidSelectIndex = 0;
                showPopupMessage("SSID Selected:", chatSSID.c_str(), "");
                delay(1000);
            } else {
                inSSIDSelectMenu = false;
                ssidSelectIndex = 0;
            }
        }
        if(isButtonPressed(BTN_BACK)) {
            inSSIDSelectMenu = false;
            ssidSelectIndex = 0;
        }
        delete[] ssidItems;
        return;
    }
    
    if(inChatMenu) {
        const char* modeItems[] = {
            "📢 Group Chat",
            "🔒 Private Chat",
            "< Back>"
        };
        int modeCount = 3;
        
        drawScrollingMenu(modeItems, modeCount, chatMenuIndex, 3, "Chat Mode", 0);
        
        if(isButtonPressed(BTN_UP)) {
            chatMenuIndex = (chatMenuIndex - 1 + modeCount) % modeCount;
        }
        if(isButtonPressed(BTN_DOWN)) {
            chatMenuIndex = (chatMenuIndex + 1) % modeCount;
        }
        if(isButtonPressed(BTN_SELECT)) {
            if(chatMenuIndex == 0) {
                currentChatMode = CHAT_MODE_GROUP;
                inChatMenu = false;
                chatMenuIndex = 0;
                showPopupMessage("✅ Group Mode", "All messages public", "");
                delay(1000);
                Serial.println("📢 GROUP CHAT MODE");
            } else if(chatMenuIndex == 1) {
                currentChatMode = CHAT_MODE_PRIVATE;
                inChatMenu = false;
                chatMenuIndex = 0;
                showPopupMessage("🔒 Private Mode", "All messages to Admin", "");
                delay(1000);
                Serial.println("🔒 PRIVATE CHAT MODE");
            } else {
                inChatMenu = false;
                chatMenuIndex = 0;
            }
        }
        if(isButtonPressed(BTN_BACK)) {
            inChatMenu = false;
            chatMenuIndex = 0;
        }
        return;
    }
    
    if(isButtonPressed(BTN_BACK)) {
        if(chatActive) stopChatAP();
        exitModule();
        return;
    }
    
    drawScrollingMenu(mainItems, count, chatMenu, 4, "Chat System", 0);
    
    if(isButtonPressed(BTN_UP)) {
        chatMenu = (chatMenu - 1 + count) % count;
    }
    if(isButtonPressed(BTN_DOWN)) {
        chatMenu = (chatMenu + 1) % count;
    }
    if(isButtonPressed(BTN_SELECT)) {
        switch(chatMenu) {
            case 0:
                startChatAP(chatSSID);
                break;
            case 1:
                stopChatAP();
                break;
            case 2:
                inSSIDSelectMenu = true;
                ssidSelectIndex = 0;
                scanAvailableSSIDs();
                break;
            case 3:
                inChatMenu = true;
                chatMenuIndex = 0;
                break;
            case 4:
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_6x10_tr);
                u8g2.drawXBMP(56, 5, 16, 16, icon_chat);
                CenteredStr(15, "Chat Status");
                char statusStr[32];
                sprintf(statusStr, "Active: %s", chatActive ? "✅" : "❌");
                CenteredStr(27, statusStr);
                sprintf(statusStr, "SSID: %s", chatSSID.c_str());
                CenteredStr(39, statusStr);
                sprintf(statusStr, "Mode: %s", currentChatMode == CHAT_MODE_GROUP ? "Group" : "Private");
                CenteredStr(51, statusStr);
                sprintf(statusStr, "Clients: %d/%d", 
                       chatActive ? WiFi.softAPgetStationNum() : 0, MAX_CLIENTS);
                CenteredStr(63, statusStr);
                u8g2.sendBuffer();
                delay(2000);
                break;
            case 5:
                exitModule();
                return;
        }
    }
    
    if(chatActive) {
        dnsServer.processNextRequest();
        webServer.handleClient();
        webSocket.loop();
        
        for(int i = 0; i < clientCount; i++) {
            if(chatClients[i].active && millis() - chatClients[i].lastSeen > 30000) {
                chatClients[i].active = false;
                String leaveMsg = "👤 " + String(chatClients[i].username) + " timed out";
                broadcastGroupMessage("system", leaveMsg);
                updateUserList();
            }
        }
    }
    
    delay(10);
}

void chatCleanup() {
    Serial.println("🧹 Chat cleanup...");
    unsigned long startTime = millis();
    
    if (chatActive) {
        stopChatAP();
        delay(100);
    }
    
    webSocket.close();
    webServer.stop();
    dnsServer.stop();
    
    chatActive = false;
    clientCount = 0;
    messageCount = 0;
    privateMsgCount = 0;
    inChatMenu = false;
    inSSIDSelectMenu = false;
    chatMenuIndex = 0;
    ssidSelectIndex = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) chatClients[i].active = false;
    for (int i = 0; i < MAX_MESSAGES; i++) chatMessages[i].used = false;
    
    clearPopup();
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    Serial.printf("✅ Chat cleanup complete (%lu ms)\n", millis() - startTime);
}