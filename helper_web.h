#ifndef HELPER_WEB_H
#define HELPER_WEB_H

/*
 * Web Interface for ENTSO-E Price Monitor
 * 
 * Provides a web dashboard with live prices, settings, and OTA update.
 * Accessible at http://pricemonitor.lan/ (or http://esp-ip/)
 * after initial configuration.
 * 
 * Pages:
 *   /           → Dashboard (price chart + status)
 *   /settings   → Edit WiFi, API key, bidding zone, timezone
 *   /update     → OTA firmware upload
 *   /reset      → Factory reset
 */

#include <Arduino.h>
#include <ESP8266WebServer.h>

extern ESP8266WebServer server;
extern Config config;
extern struct entsoe_prices PRICES;
extern int getHoursOfDay();

// Dashboard HTML page
const char dashboardHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ENTSO-E Price Monitor Dashboard</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .dashboard { max-width: 800px; width: 100%; }
    h1 { text-align: center; color: #00d4aa; font-size: 1.4em; margin-bottom: 4px; }
    .subtitle { text-align: center; color: #aaa; font-size: 0.85em; margin-bottom: 24px; }
    .card { background: #16213e; border: 1px solid #00d4aa33; border-radius: 12px; padding: 20px; margin-bottom: 16px; }
    .card h2 { font-size: 1em; color: #00d4aa; margin-bottom: 12px; }
    .bar-chart { display: flex; align-items: flex-end; gap: 6px; height: 200px; padding: 10px 0; }
    .bar-item { flex: 1; display: flex; flex-direction: column; align-items: center; }
    .bar { width: 100%; border-radius: 4px 4px 0 0; min-height: 4px; transition: height 0.3s; }
    .bar-label { font-size: 0.65em; color: #888; margin-top: 6px; text-align: center; }
    .bar-price { font-size: 0.7em; color: #aaa; margin-top: 2px; text-align: center; }
    .stats { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 12px; }
    .stat { text-align: center; padding: 8px; background: #1a1a2e; border-radius: 8px; }
    .stat-value { font-size: 1.4em; font-weight: 700; color: #00d4aa; }
    .stat-label { font-size: 0.7em; color: #888; margin-top: 2px; }
    .status-bar { display: flex; gap: 8px; flex-wrap: wrap; justify-content: center; margin-bottom: 16px; }
    .status-badge { padding: 6px 14px; border-radius: 20px; font-size: 0.75em; background: #1a1a2e; border: 1px solid #333; }
    .status-badge.online { border-color: #00b894; color: #00b894; }
    .status-badge.offline { border-color: #d63031; color: #d63031; }
    .nav { display: flex; gap: 8px; justify-content: center; flex-wrap: wrap; }
    .btn { padding: 10px 20px; border: none; border-radius: 8px; font-size: 0.85em; font-weight: 600; cursor: pointer; text-decoration: none; display: inline-block; }
    .btn-primary { background: #00d4aa; color: #1a1a2e; }
    .btn-primary:hover { background: #00b894; }
    .btn-secondary { background: #333; color: #eee; }
    .btn-secondary:hover { background: #444; }
    .btn-danger { background: #d63031; color: #fff; }
    .btn-danger:hover { background: #b71c1c; }
    .level-1 { background: #00b894; }
    .level-2 { background: #55c74d; }
    .level-3 { background: #fdcb6e; }
    .level-4 { background: #e17055; }
    .level-5 { background: #d63031; }
    .footer { text-align: center; font-size: 0.65em; color: #555; margin-top: 20px; }
    .refresh-note { text-align: center; font-size: 0.7em; color: #666; margin-top: 8px; }
  </style>
</head>
<body>
  <div class="dashboard">
    <h1>⚡ ENTSO-E Price Monitor</h1>
    <p class="subtitle">Day-ahead electricity prices — next 8 hours</p>
    
    <div class="status-bar">
      <span class="status-badge online" id="wifiBadge">WiFi Connected</span>
      <span class="status-badge online" id="apiBadge">Prices Loaded</span>
      <span class="status-badge" id="zoneBadge">Zone: NL</span>
      <span class="status-badge" id="timeBadge">--:--</span>
    </div>
    
    <div class="card">
      <h2>📊 Hourly Prices</h2>
      <div class="bar-chart" id="chart"></div>
    </div>
    
    <div class="card">
      <h2>📈 Summary</h2>
      <div class="stats" id="stats">
        <div class="stat"><div class="stat-value" id="minPrice">--</div><div class="stat-label">Min Price</div></div>
        <div class="stat"><div class="stat-value" id="maxPrice">--</div><div class="stat-label">Max Price</div></div>
        <div class="stat"><div class="stat-value" id="avgPrice">--</div><div class="stat-label">Avg Price</div></div>
        <div class="stat"><div class="stat-value" id="currentPrice">--</div><div class="stat-label">Current Hour</div></div>
      </div>
    </div>
    
    <div class="nav">
      <a href="/dashboard" class="btn btn-primary">🔄 Refresh</a>
      <a href="/settings" class="btn btn-secondary">⚙️ Settings</a>
      <a href="/update" class="btn btn-secondary">📦 OTA Update</a>
      <a href="/reset" class="btn btn-danger">🔁 Reset</a>
    </div>
    
    <p class="refresh-note">Auto-refreshes every 60 seconds</p>
    <div class="footer">ENTSO-E Price Monitor v1.2.0 · 8×8 LED Matrix</div>
  </div>
  
  <script>
    const levelColors = { 1: '#00b894', 2: '#55c74d', 3: '#fdcb6e', 4: '#e17055', 5: '#d63031' };
    const levelLabels = { 1: 'Very Cheap', 2: 'Cheap', 3: 'Normal', 4: 'Expensive', 5: 'Very Expensive' };
    
    async function loadPrices() {
      try {
        const res = await fetch('/api/prices');
        const data = await res.json();
        updateDashboard(data);
      } catch(err) {
        document.getElementById('chart').innerHTML = '<p style="color:#d63031;">Error loading prices</p>';
      }
    }
    
    function updateDashboard(data) {
      const chart = document.getElementById('chart');
      chart.innerHTML = '';
      
      let total = 0, count = 0;
      let maxH = 0;
      
      data.prices.forEach(p => {
        if (p.height > maxH) maxH = p.height;
      });
      
      data.prices.forEach((p, i) => {
        if (!p.null) {
          total += p.priceEur;
          count++;
        }
        
        const div = document.createElement('div');
        div.className = 'bar-item';
        
        const priceSpan = document.createElement('div');
        priceSpan.className = 'bar-price';
        priceSpan.textContent = p.null ? '--' : p.priceEur.toFixed(2) + '€';
        
        const bar = document.createElement('div');
        bar.className = 'bar level-' + p.level;
        const h = p.null ? 4 : Math.max(8, (p.height / 8) * 180);
        bar.style.height = h + 'px';
        
        if (i === 0) {
          bar.style.border = '2px solid #fff';
          bar.style.boxShadow = '0 0 8px rgba(0,212,170,0.5)';
        }
        
        const label = document.createElement('div');
        label.className = 'bar-label';
        label.textContent = p.label || '--:--';
        
        div.appendChild(priceSpan);
        div.appendChild(bar);
        div.appendChild(label);
        chart.appendChild(div);
      });
      
      const avg = count > 0 ? (total / count) : 0;
      
      document.getElementById('minPrice').textContent = data.minEur.toFixed(2) + '€';
      document.getElementById('maxPrice').textContent = data.maxEur.toFixed(2) + '€';
      document.getElementById('avgPrice').textContent = avg.toFixed(2) + '€';
      document.getElementById('currentPrice').textContent = data.currentEur.toFixed(2) + '€';
      
      document.getElementById('zoneBadge').textContent = 'Zone: ' + data.zone;
      document.getElementById('timeBadge').textContent = data.time;
    }
    
    // Auto-refresh every 60 seconds
    loadPrices();
    setInterval(loadPrices, 60000);
  </script>
</body>
</html>
)rawliteral";

// Settings HTML page (inline, simple form)
const char settingsHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Settings - ENTSO-E Price Monitor</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .settings { max-width: 500px; width: 100%; }
    h1 { text-align: center; color: #00d4aa; font-size: 1.3em; margin-bottom: 20px; }
    .card { background: #16213e; border: 1px solid #00d4aa33; border-radius: 12px; padding: 20px; margin-bottom: 16px; }
    label { display: block; margin-top: 14px; margin-bottom: 5px; font-weight: 600; font-size: 0.85em; color: #ccc; }
    input, select { width: 100%; padding: 10px 12px; border: 1px solid #333; border-radius: 8px; background: #1a1a2e; color: #eee; font-size: 0.9em; }
    input:focus, select:focus { outline: none; border-color: #00d4aa; }
    .info { font-size: 0.75em; color: #888; margin-top: 3px; }
    .btn { width: 100%; padding: 12px; margin-top: 20px; background: #00d4aa; color: #1a1a2e; border: none; border-radius: 8px; font-size: 1em; font-weight: 700; cursor: pointer; }
    .btn:hover { background: #00b894; }
    .status { margin-top: 12px; padding: 10px; border-radius: 6px; font-size: 0.85em; display: none; }
    .status.success { display: block; background: #00b89422; border: 1px solid #00b894; color: #00d4aa; }
    .status.error { display: block; background: #d6303122; border: 1px solid #d63031; color: #ff7675; }
    .back { text-align: center; margin-top: 12px; }
    .back a { color: #00d4aa; font-size: 0.85em; text-decoration: none; }
    .back a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <div class="settings">
    <h1>⚙️ Settings</h1>
    <div class="card">
      <form id="settingsForm">
        <label>WiFi SSID</label>
        <input type="text" id="ssid" placeholder="WiFi network name">
        
        <label>WiFi Password</label>
        <input type="password" id="password" placeholder="WiFi password">
        
        <label>ENTSO-E API Key</label>
        <input type="text" id="apiKey" placeholder="Your ENTSO-E API key">
        
        <label>Bidding Zone (EIC Code)</label>
        <input type="text" id="biddingZone" placeholder="e.g. 10YNL----------L">
        <div class="info">NL: 10YNL----------L · BE: 10YBE----------2 · DE: 10Y1001A1001A82H</div>
        
        <label>Timezone (POSIX)</label>
        <input type="text" id="timezone" placeholder="e.g. CET-1CEST,M3.5.0,M10.5.0/3">
        <div class="info">Amsterdam: CET-1CEST,... · London: GMT0 · New York: EST5EDT,...</div>
        
        <h3 style="color:#00d4aa; font-size:0.9em; margin-top:18px; border-top:1px solid #00d4aa22; padding-top:14px;">💤 Deep Sleep Schedule</h3>
        
        <label>Sleep start hour</label>
        <select id="sleepStart" style="width:100%;">
          <option value="21">21:00 (9 PM)</option>
          <option value="22">22:00 (10 PM)</option>
          <option value="23">23:00 (11 PM)</option>
          <option value="0">00:00 (Midnight)</option>
          <option value="1">01:00</option>
          <option value="2">02:00</option>
        </select>
        
        <label>Sleep end hour</label>
        <select id="sleepEnd" style="width:100%;">
          <option value="5">05:00</option>
          <option value="6">06:00</option>
          <option value="7">07:00</option>
          <option value="8">08:00</option>
          <option value="9">09:00</option>
        </select>
        <div class="info">Between these hours, the device wakes every 15 minutes to update prices but stays in low power mode.</div>
        
        <button type="submit" class="btn">Save Settings & Restart</button>
      </form>
      <div class="status" id="status"></div>
    </div>
    <div class="back"><a href="/">&larr; Back to Dashboard</a></div>
  </div>
  <script>
    const form = document.getElementById('settingsForm');
    const status = document.getElementById('status');
    
    // Load current settings
    async function loadSettings() {
      try {
        const res = await fetch('/api/config');
        const data = await res.json();
        document.getElementById('ssid').value = data.ssid || '';
        document.getElementById('apiKey').value = data.apiKey || '';
        document.getElementById('biddingZone').value = data.biddingZone || '';
        document.getElementById('timezone').value = data.timezone || '';
        if (data.sleepStart !== undefined) document.getElementById('sleepStart').value = data.sleepStart;
        if (data.sleepEnd !== undefined) document.getElementById('sleepEnd').value = data.sleepEnd;
      } catch(e) {
        status.className = 'status error';
        status.textContent = 'Error: ' + e.message;
      }
    }
    
    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      const btn = form.querySelector('.btn');
      btn.disabled = true;
      btn.textContent = 'Saving...';
      status.className = 'status';
      status.textContent = '';
      
      const data = new URLSearchParams();
      data.append('ssid', document.getElementById('ssid').value);
      data.append('password', document.getElementById('password').value);
      data.append('apiKey', document.getElementById('apiKey').value);
      data.append('biddingZone', document.getElementById('biddingZone').value);
      data.append('timezone', document.getElementById('timezone').value);
      data.append('sleepStart', document.getElementById('sleepStart').value);
      data.append('sleepEnd', document.getElementById('sleepEnd').value);
      
      try {
        const res = await fetch('/api/config', { method: 'POST', body: data });
        const text = await res.text();
        if (res.ok) {
          status.className = 'status success';
          status.textContent = '✅ ' + text;
          setTimeout(() => { window.location.href = '/'; }, 3000);
        } else {
          status.className = 'status error';
          status.textContent = '❌ ' + text;
          btn.disabled = false;
          btn.textContent = 'Save Settings & Restart';
        }
      } catch(e) {
        status.className = 'status error';
        status.textContent = '❌ ' + e.message;
        btn.disabled = false;
        btn.textContent = 'Save Settings & Restart';
      }
    });
    
    loadSettings();
  </script>
</body>
</html>
)rawliteral";

// Reset confirmation page
const char resetHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Reset - ENTSO-E Price Monitor</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .reset-box { max-width: 400px; width: 100%; text-align: center; }
    h1 { color: #d63031; font-size: 1.3em; margin-bottom: 16px; }
    .card { background: #16213e; border: 1px solid #d6303144; border-radius: 12px; padding: 24px; }
    p { font-size: 0.9em; color: #bbb; margin-bottom: 20px; line-height: 1.5; }
    .btn { padding: 12px 24px; border: none; border-radius: 8px; font-size: 0.95em; font-weight: 700; cursor: pointer; margin: 4px; }
    .btn-danger { background: #d63031; color: #fff; }
    .btn-danger:hover { background: #b71c1c; }
    .btn-secondary { background: #333; color: #eee; }
    .btn-secondary:hover { background: #444; }
    .back { margin-top: 16px; }
    .back a { color: #00d4aa; font-size: 0.85em; text-decoration: none; }
  </style>
</head>
<body>
  <div class="reset-box">
    <h1>🔁 Factory Reset</h1>
    <div class="card">
      <p>This will erase all configuration (WiFi, API key, bidding zone, timezone) and restart the device. The config portal will appear on next boot.</p>
      <button class="btn btn-danger" onclick="doReset()">Yes, Reset Everything</button>
      <a href="/" class="btn btn-secondary" style="text-decoration:none;display:inline-block;">Cancel</a>
      <div id="resetStatus" style="margin-top:12px;font-size:0.85em;color:#888;"></div>
    </div>
    <div class="back"><a href="/">&larr; Back to Dashboard</a></div>
  </div>
  <script>
    async function doReset() {
      const btn = event.target;
      btn.disabled = true;
      btn.textContent = 'Resetting...';
      document.getElementById('resetStatus').textContent = 'Clearing configuration...';
      try {
        const res = await fetch('/api/reset', { method: 'POST' });
        const text = await res.text();
        document.getElementById('resetStatus').textContent = '✅ ' + text;
        setTimeout(() => { window.location.href = '/'; }, 3000);
      } catch(e) {
        document.getElementById('resetStatus').textContent = '❌ Error: ' + e.message;
        btn.disabled = false;
        btn.textContent = 'Yes, Reset Everything';
      }
    }
  </script>
</body>
</html>
)rawliteral";

// API: return prices as JSON
void handleApiPrices() {
  String json = "{\"prices\":[";
  int currentHour = getHoursOfDay();
  float minEur = 9999, maxEur = 0, total = 0;
  int count = 0;
  
  for (int i = 0; i < 8; i++) {
    if (i > 0) json += ",";
    json += "{";
    if (!PRICES.price[i].isNull) {
      float eur = PRICES.price[i].price / 100.0;
      json += "\"null\":false,\"price\":" + String(PRICES.price[i].price);
      json += ",\"priceEur\":" + String(eur, 2);
      json += ",\"level\":" + String(PRICES.price[i].level);
      json += ",\"height\":" + String((PRICES.maximumPrice - PRICES.minimumPrice > 0) ? 
        (7 * (PRICES.price[i].price - PRICES.minimumPrice) / (PRICES.maximumPrice - PRICES.minimumPrice) + 1) : 4);
      
      int hour = (currentHour + i) % 24;
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:00", hour);
      json += ",\"label\":\"" + String(buf) + "\"";
      
      if (eur < minEur) minEur = eur;
      if (eur > maxEur) maxEur = eur;
      total += eur;
      count++;
    } else {
      json += "\"null\":true,\"price\":0,\"priceEur\":0,\"level\":3,\"height\":1,\"label\":\"--:--\"";
    }
    json += "}";
  }
  
  if (count == 0) { minEur = 0; maxEur = 0; }
  float avg = count > 0 ? total / count : 0;
  float currentEur = !PRICES.price[0].isNull ? PRICES.price[0].price / 100.0 : 0;
  
  json += "],\"minEur\":" + String(minEur, 2);
  json += ",\"maxEur\":" + String(maxEur, 2);
  json += ",\"avgEur\":" + String(avg, 2);
  json += ",\"currentEur\":" + String(currentEur, 2);
  json += ",\"zone\":\"" + String(config.biddingZone) + "\"";
  
  // Current time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[20];
    strftime(buf, sizeof(buf), "%H:%M %d/%m", &timeinfo);
    json += ",\"time\":\"" + String(buf) + "\"";
  } else {
    json += ",\"time\":\"--:--\"";
  }
  
  json += "}";
  server.send(200, "application/json", json);
}

// API: return current config
void handleApiConfig() {
  String json = "{";
  json += "\"ssid\":\"" + String(config.ssid) + "\"";
  json += ",\"apiKey\":\"" + String(config.apiKey) + "\"";
  json += ",\"biddingZone\":\"" + String(config.biddingZone) + "\"";
  json += ",\"timezone\":\"" + String(config.timezone) + "\"";
  json += ",\"sleepStart\":" + String(config.sleepStart);
  json += ",\"sleepEnd\":" + String(config.sleepEnd);
  json += ",\"configured\":" + String(config.configured ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// API: save config (POST)
void handleApiSaveConfig() {
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
  
  if (biddingZone.length() == 0) biddingZone = "10YNL----------L";
  if (timezone.length() == 0) timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
  
  // Parse sleep schedule
  String sleepStartStr = server.arg("sleepStart");
  String sleepEndStr = server.arg("sleepEnd");
  config.sleepStart = sleepStartStr.length() > 0 ? sleepStartStr.toInt() : 23;
  config.sleepEnd = sleepEndStr.length() > 0 ? sleepEndStr.toInt() : 7;
  if (config.sleepStart < 0 || config.sleepStart > 23) config.sleepStart = 23;
  if (config.sleepEnd < 0 || config.sleepEnd > 23) config.sleepEnd = 7;
  
  ssid.toCharArray(config.ssid, sizeof(config.ssid));
  password.toCharArray(config.password, sizeof(config.password));
  apiKey.toCharArray(config.apiKey, sizeof(config.apiKey));
  biddingZone.toCharArray(config.biddingZone, sizeof(config.biddingZone));
  timezone.toCharArray(config.timezone, sizeof(config.timezone));
  config.configured = true;
  
  if (saveConfig()) {
    server.send(200, "text/plain", "Settings saved! Rebooting...");
    delay(500);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Error saving configuration");
  }
}

// API: factory reset
void handleApiReset() {
  clearConfig();
  server.send(200, "text/plain", "Configuration cleared. Device will reboot...");
  delay(500);
  ESP.restart();
}

// Initialize web interface routes
void initWebInterface() {
  // Dashboard
  server.on("/", handleRoot);  // Reuse config portal HTML as dashboard
  server.on("/dashboard", []() {
    server.send(200, "text/html", dashboardHTML);
  });
  
  // API endpoints
  server.on("/api/prices", handleApiPrices);
  server.on("/api/config", HTTP_GET, handleApiConfig);
  server.on("/api/config", HTTP_POST, handleApiSaveConfig);
  server.on("/api/reset", HTTP_POST, handleApiReset);
  
  // Settings page
  server.on("/settings", []() {
    server.send(200, "text/html", settingsHTML);
  });
  
  // Reset page
  server.on("/reset", []() {
    server.send(200, "text/html", resetHTML);
  });
  
  Serial.println("Web interface initialized:");
  Serial.println("  /dashboard   - Price chart");
  Serial.println("  /settings    - Change configuration");
  Serial.println("  /api/prices  - JSON price data");
  Serial.println("  /api/config  - JSON config data");
  Serial.println("  /api/reset   - Factory reset");
}

#endif // HELPER_WEB_H
