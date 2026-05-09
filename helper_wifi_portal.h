/*
 * WiFi Configuration Portal for ENTSOE Price Monitor
 * 
 * On first boot (or when no saved config is found), this creates a WiFi access point
 * with a captive portal where the user can configure:
 *   - WiFi SSID + Password
 *   - ENTSO-E API Key
 *   - Bidding Zone
 * 
 * Configuration is saved to SPIFFS and loaded on subsequent boots.
 */

#ifndef HELPER_WIFI_PORTAL_H
#define HELPER_WIFI_PORTAL_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include "helper_led.h"

// Config file path (defined in settings.h, included via main .ino)
extern const char* configFile;

// DNS server for captive portal
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server on port 80
ESP8266WebServer server(80);

// Saved configuration
struct Config {
  char ssid[32];
  char password[64];
  char apiKey[48];
  char biddingZone[24];
  char timezone[40];
  bool configured;
};

Config config;

// Forward declarations
void startConfigPortal();
void handleRoot();
void handleSave();
bool loadConfig();
bool saveConfig();
bool hasValidConfig();
void clearConfig();
bool connectWithStoredConfig();
void disconnectWiFi();
const char* getApiKey();
const char* getBiddingZone();
const char* getTimezone();

// Load config from SPIFFS
bool loadConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS mount failed");
    return false;
  }

  if (!SPIFFS.exists(configFile)) {
    Serial.println("No config file found");
    SPIFFS.end();
    return false;
  }

  File file = SPIFFS.open(configFile, "r");
  if (!file) {
    Serial.println("Failed to open config file");
    SPIFFS.end();
    return false;
  }

  String content = file.readString();
  file.close();
  SPIFFS.end();

  // Parse the config (format: ssid|password|apiKey|biddingZone|timezone)
  int p1 = content.indexOf('|');
  int p2 = content.indexOf('|', p1 + 1);
  int p3 = content.indexOf('|', p2 + 1);
  int p4 = content.indexOf('|', p3 + 1);

  if (p1 == -1 || p2 == -1 || p3 == -1) {
    Serial.println("Invalid config format");
    return false;
  }

  content.substring(0, p1).toCharArray(config.ssid, sizeof(config.ssid));
  content.substring(p1 + 1, p2).toCharArray(config.password, sizeof(config.password));
  content.substring(p2 + 1, p3).toCharArray(config.apiKey, sizeof(config.apiKey));
  
  if (p4 == -1) {
    // Legacy format (4 fields) - assume NL timezone
    content.substring(p3 + 1).toCharArray(config.biddingZone, sizeof(config.biddingZone));
    strcpy(config.timezone, "CET-1CEST,M3.5.0,M10.5.0/3");
  } else {
    content.substring(p3 + 1, p4).toCharArray(config.biddingZone, sizeof(config.biddingZone));
    content.substring(p4 + 1).toCharArray(config.timezone, sizeof(config.timezone));
  }
  
  // Trim any whitespace/newlines
  for (int i = strlen(config.biddingZone) - 1; i >= 0; i--) {
    if (config.biddingZone[i] == '\r' || config.biddingZone[i] == '\n' || config.biddingZone[i] == ' ') {
      config.biddingZone[i] = '\0';
    } else {
      break;
    }
  }

  config.configured = true;
  Serial.println("Config loaded:");
  Serial.print("  SSID: "); Serial.println(config.ssid);
  Serial.print("  API Key: "); Serial.println(config.apiKey);
  Serial.print("  Zone: "); Serial.println(config.biddingZone);
  return true;
}

// Save config to SPIFFS
bool saveConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS mount failed");
    return false;
  }

  File file = SPIFFS.open(configFile, "w");
  if (!file) {
    Serial.println("Failed to open config file for writing");
    SPIFFS.end();
    return false;
  }

  String content = String(config.ssid) + "|" + 
                   String(config.password) + "|" + 
                   String(config.apiKey) + "|" + 
                   String(config.biddingZone) + "|" +
                   String(config.timezone);

  file.print(content);
  file.close();
  SPIFFS.end();

  Serial.println("Config saved to SPIFFS");
  return true;
}

// Check if we have a valid configuration
bool hasValidConfig() {
  if (!loadConfig()) {
    return false;
  }
  // Basic validation
  if (strlen(config.ssid) < 1 || strlen(config.apiKey) < 5) {
    return false;
  }
  return true;
}

// Clear saved config (force portal on next boot)
void clearConfig() {
  if (!SPIFFS.begin()) {
    return;
  }
  if (SPIFFS.exists(configFile)) {
    SPIFFS.remove(configFile);
    Serial.println("Config cleared");
  }
  SPIFFS.end();
  config.configured = false;
}

// Start WiFi scan (non-blocking, results fetched later via /scan)
void startWiFiScan() {
  Serial.println("Starting WiFi scan...");
  WiFi.scanNetworks(true); // async scan
}

// HTML page for the configuration portal
const char configHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ENTSO-E Price Monitor Configuration</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }
    .container { max-width: 820px; width: 100%; }
    .layout { display: flex; gap: 24px; flex-wrap: wrap; }
    .col-form { flex: 1; min-width: 320px; max-width: 420px; }
    .col-help { flex: 0 0 280px; }
    h1 { font-size: 1.4em; margin-bottom: 4px; color: #00d4aa; }
    .subtitle { font-size: 0.85em; color: #aaa; margin-bottom: 20px; }
    label { display: block; margin-top: 14px; margin-bottom: 5px; font-weight: 600; font-size: 0.85em; color: #ccc; }
    input { width: 100%; padding: 11px 14px; border: 1px solid #333; border-radius: 8px; background: #16213e; color: #eee; font-size: 0.95em; transition: border 0.2s; }
    input:focus { outline: none; border-color: #00d4aa; }
    input::placeholder { color: #555; }
    .info { font-size: 0.75em; color: #888; margin-top: 3px; }
    .btn { width: 100%; padding: 12px; margin-top: 22px; background: #00d4aa; color: #1a1a2e; border: none; border-radius: 8px; font-size: 1em; font-weight: 700; cursor: pointer; transition: background 0.2s; }
    .btn:hover { background: #00b894; }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; }
    .status { margin-top: 15px; padding: 10px; border-radius: 6px; font-size: 0.85em; display: none; }
    .status.success { display: block; background: #00b89422; border: 1px solid #00b894; color: #00d4aa; }
    .status.error { display: block; background: #d6303122; border: 1px solid #d63031; color: #ff7675; }
    .footer { text-align: center; margin-top: 24px; font-size: 0.7em; color: #555; }
    select { width: 100%; padding: 11px 14px; border: 1px solid #333; border-radius: 8px; background: #16213e; color: #eee; font-size: 0.95em; transition: border 0.2s; appearance: auto; }
    select:focus { outline: none; border-color: #00d4aa; }
    .ssid-row { display: flex; gap: 8px; }
    .ssid-row select { flex: 1; }
    .ssid-row button { flex: 0 0 auto; padding: 11px 14px; border: 1px solid #333; border-radius: 8px; background: #16213e; color: #eee; font-size: 0.95em; cursor: pointer; transition: border 0.2s; }
    .ssid-row button:hover { border-color: #00d4aa; }
    #manualSsidGroup { margin-top: 8px; padding-top: 8px; border-top: 1px dashed #333; }
    #manualSsidGroup label { margin-top: 0; font-size: 0.8em; color: #00d4aa; }
    @media (max-width: 700px) { .col-help { flex: 1 1 100%; } }
    /* Help panel */
    .help-panel { background: #16213e; border: 1px solid #00d4aa33; border-radius: 10px; padding: 20px; font-size: 0.78em; color: #bbb; line-height: 1.7; }
    .help-panel h2 { font-size: 1.1em; color: #00d4aa; margin-bottom: 14px; }
    .help-panel code { background: #00d4aa22; padding: 1px 5px; border-radius: 3px; color: #00d4aa; font-size: 0.92em; }
    .help-panel strong { color: #eee; }
    .help-panel hr { border: none; border-top: 1px solid #333; margin: 12px 0; }
    .help-panel .tip { margin-bottom: 10px; }
    .help-panel .tip-icon { color: #00d4aa; margin-right: 4px; }
    .help-badge { display: inline-block; background: #00d4aa22; color: #00d4aa; font-size: 0.7em; padding: 2px 8px; border-radius: 10px; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚡ ENTSO-E Price Monitor</h1>
    <p class="subtitle">Configure WiFi and ENTSO-E API to fetch electricity prices</p>
    
    <div class="layout">
      <!-- Left column: Configuration form -->
      <div class="col-form">
        <form id="configForm">
          <label>WiFi SSID</label>
          <div class="ssid-row">
            <select id="ssid" required>
              <option value="">-- Scanning networks... --</option>
            </select>
            <button type="button" id="scanBtn" onclick="scanNetworks()" title="Scan again">🔄 Scan</button>
          </div>
          <div class="info" id="scanInfo">Scanning for WiFi networks...</div>
          
          <div id="manualSsidGroup" style="display:none;">
            <label>Manual SSID</label>
            <input type="text" id="manualSsid" placeholder="Type network name manually">
          </div>
          
          <label>WiFi Password</label>
          <input type="password" id="password" placeholder="WiFi password">
          
          <label>ENTSO-E API Key</label>
          <input type="text" id="apiKey" placeholder="e.g. 1d9f2b3c-..." required>
          <div class="info">Get it at transparency.entsoe.eu → My Account</div>
          
          <label>Bidding Zone</label>
          <input type="text" id="biddingZone" placeholder="10YNL----------L" value="10YNL----------L">
          <div class="info">Default: Netherlands (10YNL----------L)</div>
          
          <label>Timezone</label>
          <select id="timezone">
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">🇳🇱 Amsterdam (CET/CEST) UTC+1/+2</option>
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">🇧🇪 Brussels (CET/CEST) UTC+1/+2</option>
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">🇩🇪 Berlin (CET/CEST) UTC+1/+2</option>
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">🇫🇷 Paris (CET/CEST) UTC+1/+2</option>
            <option value="GMT0">🇬🇧 London (GMT/BST) UTC+0/+1</option>
            <option value="EST5EDT,M3.2.0,M11.1.0">🇺🇸 New York (EST/EDT) UTC-5/-4</option>
            <option value="PST8PDT,M3.2.0,M11.1.0">🇺🇸 Los Angeles (PST/PDT) UTC-8/-7</option>
            <option value="CET-2CEST,M3.5.0,M10.5.0/3">🇸🇪 Stockholm (CET/CEST) UTC+2/+3</option>
            <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">🇬🇷 Athens (EET/EEST) UTC+2/+3</option>
            <option value="JST-9">🇯🇵 Tokyo (JST) UTC+9</option>
            <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">🇦🇺 Sydney (AEST/AEDT) UTC+10/+11</option>
            <option value="__custom__" style="color:#00d4aa;">✏️ Other (type POSIX string)...</option>
          </select>
          <div class="info" id="tzInfo">Select your local timezone for correct hour display.</div>
          
          <div id="manualTzGroup" style="display:none; margin-top:8px; padding-top:8px; border-top:1px dashed #333;">
            <label style="margin-top:0;font-size:0.8em;color:#00d4aa;">Custom POSIX Timezone</label>
            <input type="text" id="manualTz" placeholder="e.g. CET-1CEST,M3.5.0,M10.5.0/3">
          </div>
          
          <button type="submit" class="btn" id="saveBtn">Save &amp; Connect</button>
        </form>
        
        <div class="status" id="status"></div>
        <div class="footer">ENTSO-E Price Monitor v1.0</div>
      </div>
      
      <!-- Right column: Help panel -->
      <div class="col-help">
        <div class="help-panel">
          <h2>❓ Help &amp; Tips</h2>
          
          <div class="tip"><span class="tip-icon">📶</span> <strong>Signal bars</strong><br>
          ▂ = weak &nbsp;·&nbsp; ▂▄ = fair &nbsp;·&nbsp; ▂▄▆ = good &nbsp;·&nbsp; ▂▄▆█ = excellent<br>
          <span style="font-size:0.92em;color:#888;">Click <strong>🔄 Scan</strong> to refresh the list.</span></div>
          
          <hr>
          
          <div class="tip"><span class="tip-icon">🔑</span> <strong>ENTSO-E API Key</strong><br>
          Get it free at <strong>transparency.entsoe.eu</strong> → My Account → Token.</div>
          
          <hr>
          
          <div class="tip"><span class="tip-icon">📍</span> <strong>Bidding Zones</strong><br>
          • NL: <code>10YNL----------L</code><br>
          • BE: <code>10YBE----------2</code><br>
          • DE: <code>10Y1001A1001A82H</code><br>
          • FR: <code>10YFR-RTE------C</code><br>
          • UK: <code>10YGB----------A</code></div>
          
          <hr>
          
          <div class="tip"><span class="tip-icon">🔒</span> <strong>Hidden networks</strong><br>
          Choose <em>"Other (type manually)"</em> from the dropdown to type the SSID yourself.</div>
          
          <hr>
          
          <div class="tip"><span class="tip-icon">🌍</span> <strong>Timezone</strong><br>
          The LED matrix shows your local hours. Select your timezone so the display is correct for your location. Europe uses CET/CEST, US uses EST/EDT or PST/PDT.</div>
          
          <hr>
          
          <div class="tip"><span class="tip-icon">⚡</span> <strong>After saving</strong><br>
          The monitor will restart and connect to your WiFi. Keep the power connected!</div>
          
          <div class="help-badge">ESP8266 · 8×8 LED Matrix · ENTSO-E</div>
        </div>
      </div>
    </div>
  </div>
  
  <script>
    const form = document.getElementById('configForm');
    const status = document.getElementById('status');
    const btn = document.getElementById('saveBtn');
    const ssidSelect = document.getElementById('ssid');
    const scanInfo = document.getElementById('scanInfo');
    
    // Scan for WiFi networks
    async function scanNetworks() {
      scanInfo.textContent = 'Scanning for WiFi networks...';
      ssidSelect.innerHTML = '<option value="">-- Scanning... --</option>';
      
      try {
        const res = await fetch('/scan');
        const text = await res.text();
        
        if (text === 'scanning') {
          // Scan still in progress, try again after 1 second
          setTimeout(scanNetworks, 1000);
          return;
        }
        
        const networks = JSON.parse(text);
        
        if (networks.length === 0) {
          ssidSelect.innerHTML = '<option value="">-- No networks found --</option>';
          scanInfo.textContent = 'No WiFi networks found. Try a manual scan or check if WiFi is enabled.';
          return;
        }
        
        // Sort by signal strength (RSSI, strongest first)
        networks.sort((a, b) => b.rssi - a.rssi);
        
        // Build the select options
        let html = '<option value="">-- Select a network --</option>';
        networks.forEach(net => {
          const bars = net.rssi > -50 ? '▂▄▆█' : net.rssi > -65 ? '▂▄▆' : net.rssi > -80 ? '▂▄' : '▂';
          html += '<option value="' + net.ssid.replace(/"/g, '&quot;') + '">' + net.ssid + ' (' + bars + ')</option>';
        });
        html += '<option value="__manual__" style="color:#00d4aa;">✏️ Other (type manually)...</option>';
        ssidSelect.innerHTML = html;
        scanInfo.textContent = 'Found ' + networks.length + ' network(s). Select one above, or choose "Other" to type manually.';
        
      } catch(err) {
        scanInfo.textContent = 'Error scanning: ' + err.message;
        ssidSelect.innerHTML = '<option value="">-- Scan failed --</option>';
      }
    }
    
    // Handle custom timezone entry
    const tzSelect = document.getElementById('timezone');
    const tzInfo = document.getElementById('tzInfo');
    tzSelect.addEventListener('change', function() {
      const manualGroup = document.getElementById('manualTzGroup');
      if (this.value === '__custom__') {
        manualGroup.style.display = 'block';
        document.getElementById('manualTz').focus();
        tzInfo.textContent = 'Type a POSIX timezone string (e.g. CET-1CEST,...)';
      } else {
        manualGroup.style.display = 'none';
        tzInfo.textContent = 'Selected: ' + this.options[this.selectedIndex].text;
      }
    });
    
    // Handle manual SSID entry
    ssidSelect.addEventListener('change', function() {
      const manualGroup = document.getElementById('manualSsidGroup');
      if (this.value === '__manual__') {
        manualGroup.style.display = 'block';
        document.getElementById('manualSsid').required = true;
        document.getElementById('manualSsid').focus();
        scanInfo.textContent = 'Type your network name manually below.';
      } else {
        manualGroup.style.display = 'none';
        document.getElementById('manualSsid').required = false;
        if (this.value === '') {
          scanInfo.textContent = 'Select a network from the list.';
        } else {
          scanInfo.textContent = 'Selected: ' + this.value;
        }
      }
    });
    
    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      btn.disabled = true;
      btn.textContent = 'Saving...';
      status.className = 'status';
      status.textContent = '';
      
      // Use manual SSID if "Other" is selected
      let finalSsid = ssidSelect.value;
      if (finalSsid === '__manual__') {
        finalSsid = document.getElementById('manualSsid').value;
      }
      
      // Use custom timezone if "Other" is selected
      let finalTz = tzSelect.value;
      if (finalTz === '__custom__') {
        finalTz = document.getElementById('manualTz').value;
      }
      
      const data = new URLSearchParams();
      data.append('ssid', finalSsid);
      data.append('password', document.getElementById('password').value);
      data.append('apiKey', document.getElementById('apiKey').value);
      data.append('biddingZone', document.getElementById('biddingZone').value);
      data.append('timezone', finalTz);
      
      try {
        const res = await fetch('/save', { method: 'POST', body: data });
        const text = await res.text();
        if (res.ok) {
          status.className = 'status success';
          status.textContent = 'OK: ' + text;
          // Redirect will happen automatically
          setTimeout(() => { window.location.href = '/'; }, 3000);
        } else {
          status.className = 'status error';
          status.textContent = 'Error: ' + text;
          btn.disabled = false;
          btn.textContent = 'Save & Connect';
        }
      } catch(err) {
        status.className = 'status error';
        status.textContent = 'Error: ' + err.message;
        btn.disabled = false;
        btn.textContent = 'Save & Connect';
      }
    });
    
    // Auto-scan on page load
    scanNetworks();
  </script>
</body>
</html>
)rawliteral";

// Scan WiFi networks and return JSON list
void handleScan() {
  Serial.println("Scanning WiFi networks...");
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    // Scan still in progress
    server.send(200, "text/plain", "scanning");
    return;
  }
  
  String json = "[";
  bool first = true;
  int rssiThreshold = -90; // Skip very weak networks
  
  for (int i = 0; i < n; i++) {
    if (WiFi.RSSI(i) < rssiThreshold) continue;
    
    if (!first) json += ",";
    first = false;
    
    json += "{\"ssid\":\"";
    json += WiFi.SSID(i);
    json += "\",\"rssi\":";
    json += WiFi.RSSI(i);
    json += "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
  Serial.printf("Found %d networks, returned %d\n", n, n);
}

// Root page - show config form
void handleRoot() {
  server.send(200, "text/html", configHTML);
}

// Handle save POST request
void handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("apiKey")) {
    server.send(400, "text/plain", "SSID and API Key are required");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String apiKey = server.arg("apiKey");
  String biddingZone = server.arg("biddingZone");
  String timezone = server.arg("timezone");

  if (ssid.length() == 0 || apiKey.length() == 0) {
    server.send(400, "text/plain", "SSID and API Key cannot be empty");
    return;
  }

  if (biddingZone.length() == 0) {
    biddingZone = "10YNL----------L"; // Default NL zone
  }

  if (timezone.length() == 0) {
    timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; // Default NL timezone
  }

  ssid.toCharArray(config.ssid, sizeof(config.ssid));
  password.toCharArray(config.password, sizeof(config.password));
  apiKey.toCharArray(config.apiKey, sizeof(config.apiKey));
  biddingZone.toCharArray(config.biddingZone, sizeof(config.biddingZone));
  timezone.toCharArray(config.timezone, sizeof(config.timezone));
  config.configured = true;

  if (saveConfig()) {
    server.send(200, "text/plain", "Configuration saved! Device is rebooting...");
    Serial.println("Config saved via portal. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Error saving configuration");
  }
}

// Handle not-found (captive portal redirect)
void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// Start the configuration portal in AP mode
void startConfigPortal() {
  Serial.println("\n========================================");
  Serial.println("  NO CONFIGURATION FOUND");
  Serial.println("  Starting in AP mode...");
  Serial.println("========================================\n");

  // Start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ENTSOE-Monitor-Config");
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);

  // Start DNS server (captive portal)
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", handleScan);
  server.onNotFound(handleNotFound);
  
  // Start WiFi scan for available networks
  startWiFiScan();

  server.begin();
  Serial.println("Web server started at http://" + apIP.toString());

  // Wait for configuration (with timeout)
  unsigned long portalStart = millis();
  const unsigned long PORTAL_TIMEOUT = 300000; // 5 minutes
  
  while (!config.configured && (millis() - portalStart) < PORTAL_TIMEOUT) {
    dnsServer.processNextRequest();
    server.handleClient();
    // Show AP mode animation on LED matrix
    matrixShowAPMode();
    // Small delay to prevent watchdog issues
    delay(10);
  }

  if (!config.configured) {
    Serial.println("Portal timeout - no configuration received. Rebooting...");
    delay(1000);
    ESP.restart();
  }
}

// Try to connect to WiFi with saved config
bool connectWithStoredConfig() {
  if (!config.configured || strlen(config.ssid) < 1) {
    return false;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(config.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nWiFi connection failed");
    return false;
  }
}

// Main initialization - call from setup()
bool initWiFi() {
  Serial.println("\n--- WiFi Initialization ---");
  
  // Try to load saved config
  config.configured = false;
  
  if (loadConfig()) {
    Serial.println("Saved config found, connecting to WiFi...");
    if (connectWithStoredConfig()) {
      return true;
    } else {
      Serial.println("Could not connect with saved WiFi. Starting portal...");
      // Clear config so portal can overwrite
      clearConfig();
    }
  }

  // Start config portal
  startConfigPortal();
  
  // After portal, try connecting again
  return connectWithStoredConfig();
}

// Disconnect WiFi to save power
void disconnectWiFi() {
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disabled (power saving)");
}

// Getter functions for config values
const char* getApiKey() { return config.apiKey; }
const char* getBiddingZone() { return config.biddingZone; }
const char* getTimezone() { return config.timezone; }

#endif // HELPER_WIFI_PORTAL_H
