/*
 * ENTSO-E Price Monitor v1.1.0
 * 
 * ESP8266 (Wemos D1 Mini) with 8x8 LED Matrix
 * Displays day-ahead electricity prices from the ENTSO-E Transparency Platform.
 * Features deep sleep for power saving (~20μA in sleep).
 * 
 * How it works:
 * - On first boot: starts a WiFi config portal (captive portal) at ENTSOE-Monitor-Config
 * - User enters WiFi credentials + ENTSO-E API Key via web browser
 * - Config is saved to SPIFFS (internal flash)
 * - Every 15 minutes: wakes from deep sleep, checks if new hour
 * - New hour? Connect WiFi, fetch prices, update LED matrix, go back to sleep
 * - Same hour? Quick wake, refresh display, go back to sleep
 * 
 * Deep Sleep Wiring:
 * - Connect GPIO16 (D0) to RST pin for auto-wake
 * - Optional: Connect D7 (GPIO13) to MOSFET gate for LED power control
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
#include "helper_deep_sleep.h"

// Global prices structure (defined here, used via extern in helpers)
entsoe_prices PRICES;

// Track when we last fetched prices (updated once per hour)
int hourLastCheck = -1;

// Config done flag for deep sleep RTC persistence
bool configDone = false;

// Timer for non-blocking loop
unsigned long timeLastCheck = 0;
unsigned long intervalCheck = 1; // Run loop every second

// Wake up reason
bool wokeFromSleep = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("===========================================");
  Serial.println("   ENTSO-E Price Monitor v1.1.0");
  Serial.println("===========================================");
  
  // Determine reset reason
  rst_info *resetInfo = ESP.getResetInfoPtr();
  if (resetInfo->reason == REASON_DEEP_SLEEP_AWAKE) {
    wokeFromSleep = true;
    Serial.println("Woke from deep sleep ✓");
  } else if (resetInfo->reason == REASON_EXT_SYS_RST) {
    Serial.println("Reset by external signal (config saved)");
  } else {
    Serial.printf("Reset reason: %d (first boot or manual reset)\n", resetInfo->reason);
  }

  // Initialize LED matrix
  matrixWakeUp();

  if (wokeFromSleep) {
    // === WOKE FROM DEEP SLEEP ===
    restoreRtcData();
    
    // Also verify SPIFFS config exists
    if (!configDone || !SPIFFS.begin() || !SPIFFS.exists(configFile)) {
      // Something went wrong - restart full initialization
      Serial.println("Config not found (RTC or SPIFFS) - doing full boot");
      if (SPIFFS.begin()) SPIFFS.end();
      wokeFromSleep = false;
      // Fall through to full boot below
    } else {
      SPIFFS.end();
      Serial.println("Deep sleep wake - checking if data update needed");
      
      // Sync time from NTP (quick sync, no WiFi needed if RTC time is ok)
      initTime(config.timezone);
      int currentHour = getHoursOfDay();
      
      if (needsDataFetch(currentHour)) {
        Serial.println("New hour detected! Connecting to WiFi for fresh prices...");
        
        // Show connecting pattern
        matrixShowTest();
        
        // Connect WiFi
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
          Serial.println("WiFi connected ✓");
          showMemoryUsage();
          
          // Update time from NTP
          updateTime(config.timezone);
          currentHour = getHoursOfDay();
          
          // Fetch fresh prices
          getEntsoePrices();
          
          // Update LED matrix
          matrixShowEntsoe();
          
          // Update the tracked hour
          hourLastCheck = currentHour;
          
          // Disconnect WiFi
          disconnectWiFi();
          
          Serial.printf("Prices updated for hour %d ✓\n", currentHour);
        } else {
          Serial.println("WiFi connection failed - will retry next wake");
          matrixShowEntsoe(); // Show old prices
        }
      } else {
        Serial.println("Same hour - no data fetch needed");
        // Just display existing prices from RTC memory
        matrixShowEntsoe();
      }
      
      // Show display for a few seconds before sleeping
      Serial.println("Showing prices for 5 seconds...");
      delay(5000);
      
      // Enter deep sleep
      enterDeepSleep();
    }
  }

  // === FULL BOOT (first boot or config portal) ===
  if (!wokeFromSleep) {
    Serial.println("\n--- Full boot sequence ---");
    
    // Initialize the LED matrix
    matrixInitialize();
    
    // Show test pattern
    matrixShowTest();
    
    // Initialize WiFi (with config portal on first boot)
    initWiFi();
    
    // After config portal: configDone is set
    configDone = true;
    
    // Confirm AP mode is done
    matrixShowTest();
    
    // Get time from NTP
    initTime(config.timezone);
    
    // Fetch prices
    Serial.println("Fetching initial data from ENTSO-E...");
    getEntsoePrices();
    matrixShowEntsoe();
    
    // Update time again
    updateTime(config.timezone);
    
    // Disconnect WiFi
    disconnectWiFi();
    
    // Mark hour as checked
    hourLastCheck = getHoursOfDay();
    
    Serial.println("Setup complete! Showing prices for 5 seconds...");
    delay(5000);
    
    // Save state and enter deep sleep
    enterDeepSleep();
  }
}

void loop() {
  // Main loop - only reached for config portal mode (before deep sleep starts)
  // After enterDeepSleep(), the ESP shuts down and won't reach here
  
  unsigned long timeNow = millis();
  
  if (timeLastCheck == 0 || (unsigned long)(timeNow - timeLastCheck) > 1000) {
    timeLastCheck = timeNow;
    
    // Update LED matrix (non-blocking AP mode animation)
    matrixUpdateAPMode();
    
    // Process portal requests if WiFi is in AP mode
    dnsServer.processNextRequest();
    server.handleClient();
  }
}
