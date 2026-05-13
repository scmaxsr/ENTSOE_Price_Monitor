#ifndef HELPER_DEEP_SLEEP_H
#define HELPER_DEEP_SLEEP_H

/*
 * Deep Sleep Helper for ESP8266
 * 
 * Manages deep sleep to save power:
 * - ESP wakes every SLEEP_INTERVAL_MIN minutes
 * - RTC memory preserves price data during sleep
 * - LED matrix is turned off during sleep to save power
 * - Only connects to WiFi if a new hour has started
 * 
 * Power savings:
 * - Normal running: ~80mA (with WiFi + LED matrix)
 * - Deep sleep (LED off, WiFi off): ~20μA (0.02mA)
 * - Estimated battery life with 2000mAh: ~11 years (vs ~25 hours without sleep)
 * - Realistic with hourly wake for ~3 sec: ~100+ days on 2000mAh
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <user_interface.h>

#include "helper_led.h"

// Sleep interval in minutes
#ifndef SLEEP_INTERVAL_MIN
  #define SLEEP_INTERVAL_MIN 15
#endif

// Convert to microseconds (ESP8266 deep sleep uses microseconds)
#define SLEEP_TIME_US (SLEEP_INTERVAL_MIN * 60 * 1000000ULL)

// RTC memory slot for storing data during deep sleep
// ESP8266 has 128 bytes of RTC memory (slots 64-127 are user-available)
// We store our data in slot 64 (4 bytes per slot)
#define RTC_SLOT_HOUR_CHECKED 64   // Stores hourLastCheck
#define RTC_SLOT_CONFIG_CHECKED 65 // Stores whether config has been done

// Pin to control LED matrix power during sleep
// We use D0 (GPIO16) - but this is also the wake pin!
// Alternative: use D7 (GPIO13) to control a MOSFET for LED power
#ifndef LED_POWER_PIN
  #define LED_POWER_PIN 13  // D7 on Wemos D1 Mini
#endif

// Forward declarations
extern int hourLastCheck;
extern bool configDone;
extern Config config;

// Variables preserved across deep sleep (restored from RTC on boot)
int savedHourLastCheck = -1;
bool wasConfigDone = false;

// Save configuration state to RTC memory
void saveRtcData() {
  system_rtc_mem_write(RTC_SLOT_HOUR_CHECKED, &hourLastCheck, sizeof(hourLastCheck));
  system_rtc_mem_write(RTC_SLOT_CONFIG_CHECKED, &configDone, sizeof(configDone));
  
  Serial.printf("RTC saved: hourLastCheck=%d, configDone=%d\n", hourLastCheck, configDone);
}

// Restore configuration state from RTC memory
void restoreRtcData() {
  // Read hourLastCheck from RTC, if it fails (first boot), use default
  if (system_rtc_mem_read(RTC_SLOT_HOUR_CHECKED, &savedHourLastCheck, sizeof(savedHourLastCheck))) {
    hourLastCheck = savedHourLastCheck;
    Serial.printf("RTC restore: hourLastCheck=%d\n", hourLastCheck);
  } else {
    hourLastCheck = -1;
    Serial.println("RTC: first boot or RTC reset");
  }
  
  // Read configDone from RTC
  if (system_rtc_mem_read(RTC_SLOT_CONFIG_CHECKED, &wasConfigDone, sizeof(wasConfigDone))) {
    configDone = wasConfigDone;
    Serial.printf("RTC restore: configDone=%d\n", configDone);
  } else {
    configDone = false;
  }
}

// Turn LED matrix power on/off
// We use a MOSFET on the LED power line controlled by LED_POWER_PIN
// This completely disconnects power from the LEDs during sleep
void setLedPower(bool on) {
  pinMode(LED_POWER_PIN, OUTPUT);
  digitalWrite(LED_POWER_PIN, on ? HIGH : LOW);
  
  if (on) {
    Serial.println("LED power: ON");
  } else {
    Serial.println("LED power: OFF");
  }
}

// Prepare for deep sleep:
// 1. Save state to RTC memory
// 2. Turn off LED matrix
// 3. Disconnect WiFi (should already be disconnected)
// 4. Go to deep sleep
void enterDeepSleep() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("Entering Deep Sleep...");
  Serial.printf("Sleep interval: %d minutes\n", SLEEP_INTERVAL_MIN);
  Serial.println("========================================");
  
  // Save current state to RTC memory
  saveRtcData();
  
  // Turn off LED matrix
  matrixPowerOff();
  
  // Short delay to let Serial message finish
  delay(100);
  
  // Detach GPIO pins to reduce power leakage
  pinMode(LED_POWER_PIN, INPUT_PULLDOWN_16); // Pull low during sleep
  
  // Enter deep sleep
  // ESP8266 will wake up after SLEEP_TIME_US microseconds
  // GPIO16 (D0) must be connected to RST pin for auto-wake
  ESP.deepSleep(SLEEP_TIME_US, WAKE_RFCAL);
  
  // deepSleep does not return - esp goes to sleep immediately
  delay(100); // Won't reach here, but just in case
}

// Check if we should wake up for data fetch
// Returns true if a new hour has started (based on RTC data)
bool shouldWakeForData() {
  // If we haven't checked any hour yet, we need data
  if (hourLastCheck < 0 || hourLastCheck > 23) return true;
  
  // We wake up every SLEEP_INTERVAL_MIN minutes to check
  // Return true to check time, false to go back to sleep
  return true; // We'll check time after waking
}

// Check if current hour is within the sleep schedule
// Returns true if we should go into deep sleep immediately
// Sleep schedule: from config.sleepStart to config.sleepEnd (e.g. 23:00 - 07:00)
bool isSleepTime(int currentHour) {
  int sleepStart = config.sleepStart;
  int sleepEnd = config.sleepEnd;
  
  // Validate (should never happen, but protect against bad config)
  if (sleepStart < 0 || sleepStart > 23) sleepStart = 23;
  if (sleepEnd < 0 || sleepEnd > 23) sleepEnd = 7;
  
  // Handle overnight schedule (e.g. 23:00 - 07:00)
  if (sleepStart < sleepEnd) {
    // Same-day schedule (e.g. 01:00 - 06:00)
    return (currentHour >= sleepStart && currentHour < sleepEnd);
  } else {
    // Overnight schedule (e.g. 23:00 - 07:00)
    return (currentHour >= sleepStart || currentHour < sleepEnd);
  }
}

// Call this after boot to determine if we need a full WiFi fetch
// or can just display cached data and go back to sleep
bool needsDataFetch(int currentHour) {
  // First boot or data expired
  if (hourLastCheck < 0) return true;
  
  // New hour started
  if (currentHour != hourLastCheck) return true;
  
  // We already have data for this hour
  return false;
}

#endif // HELPER_DEEP_SLEEP_H
