#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// Default settings (overwritten by config from SPIFFS)
static const char* default_ssid = "YourWiFiSSID";
static const char* default_password = "YourWiFiPassword";
static const char* default_apiKey = "YOUR_ENTSOE_API_KEY";
static const char* default_biddingZone = "10YNL----------L";
const char* entsoeApi = "https://web-api.tp.entsoe.eu/api";
const uint16_t port = 443;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const String timeZoneNL = "CET-1CEST,M3.5.0,M10.5.0/3";
const int ledBrightness = 20;
const char* configFile = "/config.txt";
const char* portalDomain = "pricemonitor";

#endif
