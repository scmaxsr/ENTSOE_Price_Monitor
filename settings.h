#ifndef SETTINGS_H
#define SETTINGS_H

// Default settings - these are used when no saved config exists yet
// The WiFi config portal will allow changing these at first boot

// Default WiFi (will be overwritten by saved config)
static const char* default_ssid = "YourWiFiSSID";
static const char* default_password = "YourWiFiPassword";

// ENTSO-E API Settings (will be overwritten by saved config)
static const char* default_apiKey = "YOUR_ENTSOE_API_KEY";
static const char* default_biddingZone = "10YNL----------L";

// ENTSO-E API endpoint
const char* entsoeApi = "https://web-api.tp.entsoe.eu/api";
const uint16_t port = 443;

// NTP Settings
static const char* ntpServer = "pool.ntp.org";
static const long  gmtOffset_sec = 3600;
static const int   daylightOffset_sec = 3600;

// Timezone for the Netherlands (CET/CEST)
const String timeZoneNL = "CET-1CEST,M3.5.0,M10.5.0/3";

// LED Matrix Settings
const int ledBrightness = 20;

// Config file in SPIFFS
static const char* configFile = "/config.txt";
const char* portalDomain = "pricemonitor.lan";

#endif // SETTINGS_H
