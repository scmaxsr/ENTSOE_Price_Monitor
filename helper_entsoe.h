#ifndef HELPER_ENTSOE_H
#define HELPER_ENTSOE_H

/*
 * ENTSO-E API Helper
 * 
 * Fetches day-ahead electricity prices from the ENTSO-E Transparency Platform API.
 * Parses the XML response and stores prices in a structure compatible with the LED matrix.
 * 
 * Note: ENTSO-E returns XML by default. On ESP8266 we parse it with simple string
 * extraction to avoid the overhead of a full XML library.
 */

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>

// Forward declarations from helper_time.h
extern int getHoursOfDay();

// A single price valid for one hour
struct entsoe_price {
  char starttime[14];   // e.g. "20260506T1800"
  int price;            // price in 0.1 eurocent / MWh (scaled for LED display)
  int level;            // Price Level (1-5) - calculated based on min/max
  boolean isNull;       // True if data not available
};

// Prices for the next 8 hours (for 8x8 LED matrix columns)
struct entsoe_prices {
  entsoe_price price[8];
  int minimumPrice = 100000;  // min price in 0.1 eurocent
  int maximumPrice = 0;        // max price in 0.1 eurocent
};

// Global prices structure
entsoe_prices PRICES;

// ENTSO-E API base URL is defined in settings.h
extern const char* entsoeApi;
extern const char* getApiKey();
extern const char* getBiddingZone();

// Extract substring between two markers (simple XML parser helper)
String extractBetween(const String& data, const String& startMarker, const String& endMarker, int startPos = 0) {
  int startIdx = data.indexOf(startMarker, startPos);
  if (startIdx == -1) return "";
  startIdx += startMarker.length();
  int endIdx = data.indexOf(endMarker, startIdx);
  if (endIdx == -1) return "";
  return data.substring(startIdx, endIdx);
}

// Print prices over Serial
void printPrices() {
  Serial.println("--- ENTSO-E Prices ---");
  for (int i = 0; i < 8; i++) {
    if (!PRICES.price[i].isNull) {
      Serial.printf("  %s: %i (%i)\n", PRICES.price[i].starttime, PRICES.price[i].price, PRICES.price[i].level);
    } else {
      Serial.printf("  Hour %d: No data\n", i);
    }
  }
  Serial.printf("Min: %i  Max: %i\n", PRICES.minimumPrice, PRICES.maximumPrice);
}

// Calculate price levels (1-5) based on min/max range
void calculateLevels() {
  int range = PRICES.maximumPrice - PRICES.minimumPrice;
  if (range <= 0) {
    // All prices are the same - set all to level 3 (normal)
    for (int i = 0; i < 8; i++) {
      if (!PRICES.price[i].isNull) {
        PRICES.price[i].level = 3;
      }
    }
    return;
  }

  for (int i = 0; i < 8; i++) {
    if (!PRICES.price[i].isNull) {
      float pct = (float)(PRICES.price[i].price - PRICES.minimumPrice) / range;
      if (pct < 0.2)      PRICES.price[i].level = 1; // Very cheap
      else if (pct < 0.4) PRICES.price[i].level = 2; // Cheap
      else if (pct < 0.6) PRICES.price[i].level = 3; // Normal
      else if (pct < 0.8) PRICES.price[i].level = 4; // Expensive
      else                 PRICES.price[i].level = 5; // Very expensive
    }
  }
}

// Parse ENTSO-E XML response and extract prices for next 8 hours
void parseEntsoeXml(const String& xmlResponse) {
  Serial.println("Parsing ENTSO-E XML response...");
  
  // Reset prices
  PRICES.minimumPrice = 100000;
  PRICES.maximumPrice = 0;
  for (int i = 0; i < 8; i++) {
    PRICES.price[i].isNull = true;
    PRICES.price[i].price = 0;
    PRICES.price[i].level = 3;
  }

  // We need to find all TimeSeries blocks and look for the one with A03 (mixed/aggregate)
  // or simply find all point/price.amount pairs
  
  int currentHour = getHoursOfDay();
  if (currentHour < 0) currentHour = 0;
  
  Serial.printf("Current hour: %d\n", currentHour);

  // Count how many <Point> blocks we have
  int pointCount = 0;
  int searchPos = 0;
  while (true) {
    int pos = xmlResponse.indexOf("<Point>", searchPos);
    if (pos == -1) break;
    pointCount++;
    searchPos = pos + 7;
  }
  
  Serial.printf("Found %d price points\n", pointCount);
  if (pointCount == 0) {
    Serial.println("ERROR: No price points found in XML!");
    return;
  }

  // Extract all price.amount and position values
  // We look for position and price.amount pairs
  searchPos = 0;
  int positions[48]; // max 48 hours
  int prices[48];
  int count = 0;
  
  while (count < 48) {
    int posStart = xmlResponse.indexOf("<position>", searchPos);
    int priceStart = xmlResponse.indexOf("<price.amount>", searchPos);
    
    if (posStart == -1 || priceStart == -1) break;
    
    // Find which comes first
    int nextPoint = xmlResponse.indexOf("<Point>", searchPos);
    if (nextPoint == -1) break;
    
    // Extract position
    int posEnd = xmlResponse.indexOf("</position>", posStart);
    String posStr = xmlResponse.substring(posStart + 10, posEnd);
    posStr.trim();
    positions[count] = posStr.toInt();
    
    // Extract price.amount after this position
    int priceStartLocal = xmlResponse.indexOf("<price.amount>", posStart);
    if (priceStartLocal == -1) break;
    int priceEnd = xmlResponse.indexOf("</price.amount>", priceStartLocal);
    String priceStr = xmlResponse.substring(priceStartLocal + 14, priceEnd);
    priceStr.trim();
    
    // Price is in EUR/MWh, convert to 0.1 eurocent for consistency with original
    // EUR/MWh → 0.1 eurocent: multiply by 1000 (1 EUR/MWh = 0.1 eurocent/kWh ≈ scale factor)
    // Actually let's keep it simple: store as integer cents per MWh * 10
    float priceFloat = priceStr.toFloat();
    prices[count] = (int)(priceFloat * 100); // Store as 0.1 EUR/MWh
    
    Serial.printf("  Point %d: position=%d, price=%.2f EUR/MWh (%d * 0.1)\n", 
                  count, positions[count], priceFloat, prices[count]);
    
    count++;
    searchPos = priceEnd + 15; // Move past </price.amount>
  }

  if (count == 0) {
    Serial.println("ERROR: Could not extract any prices!");
    return;
  }

  // ENTSO-E uses 1-based position (1 = 00:00-01:00, 2 = 01:00-02:00, etc. in UTC)
  // We need the next 8 hours starting from current hour
  // The data is typically for a full day (midnight to midnight UTC)
  
  // Find the prices for currentHour through currentHour+7
  for (int i = 0; i < 8; i++) {
    int targetHour = (currentHour + i) % 24;
    // ENTSO-E position is 1-based (position 1 = hour starting at 00:00 UTC)
    int targetPosition = targetHour + 1;
    
    // Find matching position
    for (int j = 0; j < count; j++) {
      if (positions[j] == targetPosition) {
        PRICES.price[i].isNull = false;
        PRICES.price[i].price = prices[j];
        PRICES.price[i].level = 3; // Will be recalculated
        
        // Format starttime like original: YYYYMMDDTHH00
        // We'll just store the hour info
        snprintf(PRICES.price[i].starttime, 14, "2026%02d%02dT%02d00", 
                 5, 7, targetHour); // Approximate date
        
        // Update min/max
        if (prices[j] < PRICES.minimumPrice) PRICES.minimumPrice = prices[j];
        if (prices[j] > PRICES.maximumPrice) PRICES.maximumPrice = prices[j];
        
        break;
      }
    }
  }

  // Calculate price levels based on min/max
  calculateLevels();
  
  printPrices();
}

// Get the date string for today and tomorrow (YYYYMMDD format)
String getDateString(int daysFromNow) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "20260101";
  }
  
  // Add days
  time_t t = mktime(&timeinfo) + (daysFromNow * 86400);
  struct tm* newTime = gmtime(&t);
  
  char buf[9];
  strftime(buf, sizeof(buf), "%Y%m%d", newTime);
  return String(buf);
}

// Fetch prices from ENTSO-E API
void getEntsoePrices() {
  Serial.println("\n--- Fetching ENTSO-E Prices ---");
  
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); // Accept any certificate
  
  HTTPClient https;
  https.useHTTP10(true);
  
  // Build URL for day-ahead prices
  String today = getDateString(0);
  String tomorrow = getDateString(1);
  
  // ENTSO-E API: get day-ahead prices for today and tomorrow
  String url = String(entsoeApi) + 
               "?securityToken=" + getApiKey() +
               "&documentType=A44" +
               "&in_Domain=" + getBiddingZone() +
               "&out_Domain=" + getBiddingZone() +
               "&periodStart=" + today + "0000" +
               "&periodEnd=" + tomorrow + "0000";
  
  Serial.print("URL: ");
  Serial.println(url);
  
  Serial.print("[HTTPS] GET...\n");
  if (https.begin(*client, url)) {
    // Try with accept header for JSON first (falls back to XML)
    https.addHeader("Accept", "application/xml");
    
    int httpCode = https.GET();
    Serial.printf("HTTP Response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      
      Serial.printf("Response length: %d bytes\n", response.length());
      
      // Debug: show first 200 chars
      Serial.print("Response preview: ");
      Serial.println(response.substring(0, 200));
      
      parseEntsoeXml(response);
    } else {
      Serial.printf("HTTP Error: %s\n", https.errorToString(httpCode).c_str());
      
      // Try to read error response
      if (httpCode > 0) {
        String errorResponse = https.getString();
        Serial.print("Error response: ");
        Serial.println(errorResponse.substring(0, 500));
      }
    }
    https.end();
  } else {
    Serial.println("[HTTPS] Unable to connect to ENTSO-E API");
  }
  
  Serial.println("--- ENTSO-E Fetch Complete ---\n");
}

#endif // HELPER_ENTSOE_H
