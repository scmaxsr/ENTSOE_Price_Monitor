/*
 * ENTSO-E Price Monitor
 * 
 * ESP8266 (Wemos D1 Mini) with 8x8 LED Matrix
 * Displays day-ahead electricity prices from the ENTSO-E Transparency Platform.
 * 
 * How it works:
 * - On first boot: starts a WiFi config portal (captive portal) at ENTSOE-Monitor-Config
 * - User enters WiFi credentials + ENTSO-E API Key via web browser
 * - Config is saved to SPIFFS (internal flash)
 * - Every hour: fetches day-ahead prices, updates LED bargraph
 * - LED shows current hour + next 7 hours as colored bars
 * 
 * Hardware: Wemos D1 Mini (ESP8266) + 8x8 NeoPixel LED Matrix
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "settings.h"
#include "helper_wifi_portal.h"
#include "helper_time.h"
#include "helper_memory.h"
#include "helper_entsoe.h"
#include "helper_led.h"

// Track when we last fetched prices (updated once per hour)
int hourLastCheck = -1;

// Timer for non-blocking loop
unsigned long timeLastCheck = 0;
unsigned long intervalCheck = 1; // Run loop every second

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  ENTSO-E Price Monitor v1.0");
  Serial.println("========================================");
  
    // Initialize the LED matrix
  matrixInitialize();
  
  // Show a quick test pattern
  matrixShowTest();
  
  // Initialize WiFi (automatically starts config portal if no saved config)
  initWiFi();
  
  // Show test pattern again to confirm exit from AP mode
  matrixShowTest();
  
  // Get time from NTP and set timezone (NL)
  initTime(timeZoneNL);
  
  // Show connecting animation while we fetch initial data
  Serial.println("Fetching initial data from ENTSO-E...");
  
  // Force initial data fetch
  getEntsoePrices();
  matrixShowEntsoe();
  
  // Update time again after API call
  updateTime(timeZoneNL);
  
  // Disconnect WiFi to save power
  disconnectWiFi();
  
  // Mark current hour as checked
  hourLastCheck = getHoursOfDay();
  timeLastCheck = 0; // Reset timer so loop runs immediately
  
  Serial.println("Setup complete - monitor is running!");
}

void loop() {
  unsigned long timeNow = millis();
  
  // Run main loop once per second
  if (timeLastCheck == 0 || (unsigned long)(timeNow - timeLastCheck) > (intervalCheck * 1000)) {
    timeLastCheck = timeNow;
    
    int hourNow = getHoursOfDay();
    
    // Check if we need new data (new hour started, or wrapped past midnight)
    if (hourNow > hourLastCheck || (hourNow == 0 && hourLastCheck == 23)) {
      
      Serial.printf("\n--- New hour: %d ---\n", hourNow);
      
      // Only fetch if WiFi is available
      if (WiFi.status() == WL_CONNECTED) {
        
        // Fetch fresh prices from ENTSO-E
        getEntsoePrices();
        
        // Update the LED matrix
        matrixShowEntsoe();
        
        // Update time from NTP
        updateTime(timeZoneNL);
        
        // Mark this hour as checked
        hourLastCheck = hourNow;
        
        // Disconnect WiFi to save power
        disconnectWiFi();
        
      } else {
        Serial.println("WiFi not available - reconnecting...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.password);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
          delay(500);
          Serial.print(".");
          attempts++;
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
          // Retry data fetch
          getEntsoePrices();
          matrixShowEntsoe();
          disconnectWiFi();
          hourLastCheck = hourNow;
        }
      }
    }
    
    // Update the LED matrix regularly (needed for blinking current hour indicator)
    // This runs every second as part of the main loop
    matrixShowEntsoe();
  }
}
