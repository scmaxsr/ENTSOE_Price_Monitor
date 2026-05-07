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
#include "settings.h"

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

  // Parse the config (format: ssid|password|apiKey|biddingZone)
  int p1 = content.indexOf('|');
  int p2 = content.indexOf('|', p1 + 1);
  int p3 = content.indexOf('|', p2 + 1);

  if (p1 == -1 || p2 == -1 || p3 == -1) {
    Serial.println("Invalid config format");
    return false;
  }

  content.substring(0, p1).toCharArray(config.ssid, sizeof(config.ssid));
  content.substring(p1 + 1, p2).toCharArray(config.password, sizeof(config.password));
  content.substring(p2 + 1, p3).toCharArray(config.apiKey, sizeof(config.apiKey));
  content.substring(p3 + 1).toCharArray(config.biddingZone, sizeof(config.biddingZone));
  
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
                   String(config.biddingZone);

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

// HTML page for the configuration portal
const char configHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ENTSO-E Prijs Monitor Configuratie</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .container { max-width: 420px; width: 100%; padding: 20px; }
    h1 { font-size: 1.4em; margin-bottom: 8px; color: #00d4aa; }
    p { font-size: 0.85em; color: #aaa; margin-bottom: 20px; }
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
    .hint { background: #00d4aa11; border-left: 3px solid #00d4aa; padding: 10px; border-radius: 4px; margin-top: 18px; font-size: 0.8em; color: #bbb; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚡ ENTSO-E Prijs Monitor</h1>
    <p>Configureer WiFi en ENTSO-E API om prijzen op te halen</p>
    
    <form id="configForm">
      <label>WiFi SSID</label>
      <input type="text" id="ssid" placeholder="b.v. Ziggo-1234" required>
      
      <label>WiFi Wachtwoord</label>
      <input type="password" id="password" placeholder="WiFi wachtwoord">
      
      <label>ENTSO-E API Key</label>
      <input type="text" id="apiKey" placeholder="b.v. 1d9f2b3c-..." required>
      <div class="info">Verkrijgbaar via transparency.entsoe.eu → My Account</div>
      
      <label>Bidding Zone</label>
      <input type="text" id="biddingZone" placeholder="10YNL----------L" value="10YNL----------L">
      <div class="info">Standaard Nederland (10YNL----------L)</div>
      
      <button type="submit" class="btn" id="saveBtn">Opslaan &amp; Verbinden</button>
    </form>
    
    <div class="hint">⚠️ Na opslaan start de monitor opnieuw. Houd de stroom aangesloten.</div>
    
    <div class="status" id="status"></div>
    
    <div class="footer">ENTSO-E Prijs Monitor v1.0</div>
  </div>
  
  <script>
    const form = document.getElementById('configForm');
    const status = document.getElementById('status');
    const btn = document.getElementById('saveBtn');
    
    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      btn.disabled = true;
      btn.textContent = 'Bezig met opslaan...';
      status.className = 'status';
      status.textContent = '';
      
      const data = new URLSearchParams();
      data.append('ssid', document.getElementById('ssid').value);
      data.append('password', document.getElementById('password').value);
      data.append('apiKey', document.getElementById('apiKey').value);
      data.append('biddingZone', document.getElementById('biddingZone').value);
      
      try {
        const res = await fetch('/save', { method: 'POST', body: data });
        const text = await res.text();
        if (res.ok) {
          status.className = 'status success';
          status.textContent = '✅ ' + text;
          // Redirect will happen automatically
          setTimeout(() => { window.location.href = '/'; }, 3000);
        } else {
          status.className = 'status error';
          status.textContent = '❌ ' + text;
          btn.disabled = false;
          btn.textContent = 'Opslaan & Verbinden';
        }
      } catch(err) {
        status.className = 'status error';
        status.textContent = '❌ Fout bij verzenden: ' + err.message;
        btn.disabled = false;
        btn.textContent = 'Opslaan & Verbinden';
      }
    });
  </script>
</body>
</html>
)rawliteral";

// Root page - show config form
void handleRoot() {
  server.send(200, "text/html", configHTML);
}

// Handle save POST request
void handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("apiKey")) {
    server.send(400, "text/plain", "SSID en API Key zijn verplicht");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String apiKey = server.arg("apiKey");
  String biddingZone = server.arg("biddingZone");

  if (ssid.length() == 0 || apiKey.length() == 0) {
    server.send(400, "text/plain", "SSID en API Key mogen niet leeg zijn");
    return;
  }

  if (biddingZone.length() == 0) {
    biddingZone = "10YNL----------L"; // Default NL zone
  }

  ssid.toCharArray(config.ssid, sizeof(config.ssid));
  password.toCharArray(config.password, sizeof(config.password));
  apiKey.toCharArray(config.apiKey, sizeof(config.apiKey));
  biddingZone.toCharArray(config.biddingZone, sizeof(config.biddingZone));
  config.configured = true;

  if (saveConfig()) {
    server.send(200, "text/plain", "Configuratie opgeslagen! Apparaat start opnieuw...");
    Serial.println("Config saved via portal. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Fout bij opslaan configuratie");
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
  Serial.println("  GEEN CONFIGURATIE GEVONDEN");
  Serial.println("  Starten in AP-modus...");
  Serial.println("========================================\n");

  // Start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ENTSOE-Monitor-Config");
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP adres: ");
  Serial.println(apIP);

  // Start DNS server (captive portal)
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Webserver gestart op http://" + apIP.toString());

  // Wait for configuration (with timeout)
  unsigned long portalStart = millis();
  const unsigned long PORTAL_TIMEOUT = 300000; // 5 minutes
  
  while (!config.configured && (millis() - portalStart) < PORTAL_TIMEOUT) {
    dnsServer.processNextRequest();
    server.handleClient();
    // Small delay to prevent watchdog issues
    delay(10);
  }

  if (!config.configured) {
    Serial.println("Portal timeout - geen configuratie ontvangen. Opnieuw opstarten...");
    delay(1000);
    ESP.restart();
  }
}

// Try to connect to WiFi with saved config
bool connectWithStoredConfig() {
  if (!config.configured || strlen(config.ssid) < 1) {
    return false;
  }

  Serial.print("Verbinden met WiFi: ");
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
    Serial.println("\nWiFi verbonden!");
    Serial.print("IP adres: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nWiFi verbinding mislukt");
    return false;
  }
}

// Main initialization - call from setup()
bool initWiFi() {
  Serial.println("\n--- WiFi Initialisatie ---");
  
  // Try to load saved config
  config.configured = false;
  
  if (loadConfig()) {
    Serial.println("Opgeslagen config gevonden, verbinden met WiFi...");
    if (connectWithStoredConfig()) {
      return true;
    } else {
      Serial.println("Kon niet verbinden met opgeslagen WiFi. Portal starten...");
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
  Serial.println("WiFi uitgeschakeld (energiebesparing)");
}

// Getter functions for config values
const char* getApiKey() { return config.apiKey; }
const char* getBiddingZone() { return config.biddingZone; }

#endif // HELPER_WIFI_PORTAL_H
