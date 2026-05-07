#ifndef HELPER_LED_H
#define HELPER_LED_H

/*
 * LED Matrix Display Helper
 * 
 * Controls an 8x8 NeoPixel LED matrix to display electricity prices as a bar chart.
 * Each column represents one hour. Height = relative price, Color = price level.
 * 
 * Color mapping:
 *   Level 1 (Very Cheap):  Green
 *   Level 2 (Cheap):        Yellow-Green
 *   Level 3 (Normal):        Amber/Yellow
 *   Level 4 (Expensive):    Orange
 *   Level 5 (Very Expensive): Red
 */

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// MATRIX DECLARATION:
// Pin D3 = GPIO0 on Wemos D1 Mini
// Adjust pin if needed for your board
#ifndef D3
  #define D3 0
#endif

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, D3,
  NEO_MATRIX_BOTTOM     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

// Colors for price levels (1-5)
// Level 1: Bright Green     - Very cheap
// Level 2: Yellow-Green     - Cheap
// Level 3: Yellow/Amber     - Normal
// Level 4: Orange           - Expensive
// Level 5: Red              - Very expensive
const uint16_t colors[] = {
  matrix.Color(0, 255, 0),    // Level 1: Green
  matrix.Color(85, 255, 0),   // Level 2: Yellow-Green
  matrix.Color(255, 180, 0),  // Level 3: Amber/Yellow
  matrix.Color(255, 80, 0),   // Level 4: Orange
  matrix.Color(255, 0, 0)     // Level 5: Red
};

// Blinking state for current hour indicator
bool blinkState = false;

// External reference to price data
extern struct entsoe_prices PRICES;
extern int getHoursOfDay();
extern const int ledBrightness;

void matrixInitialize() {
  Serial.println("Initialize LED Matrix");
  matrix.begin();
  matrix.setBrightness(ledBrightness);
  matrix.fillScreen(0);
  matrix.show();
  blinkState = false;
}

// Function to remap pixels with horizontal and vertical flipping
// Adjust this if your matrix orientation is different
void drawPixelRemapped(int x, int y, uint16_t color) {
  // Flip the x-coordinate for horizontal reversal
  x = 7 - x; // Assuming an 8x8 matrix
  // Flip the y-coordinate for vertical reversal
  if (x % 2 == 0) {
    y = 7 - y; // Even columns
  }
  matrix.drawPixel(x, y, color);
}

// Draw a column with specified height and color
void matrixLine(int column, int height, uint16_t color) {
  for (int y = 0; y < height; y++) {
    drawPixelRemapped(column, 7 - y, color); // Start from bottom, grow upwards
  }
}

// Show test pattern on startup
void matrixShowTest() {
  matrix.fillScreen(0);
  matrixLine(0, 1, colors[0]);
  matrixLine(1, 2, colors[1]);
  matrixLine(2, 3, colors[2]);
  matrixLine(3, 4, colors[3]);
  matrixLine(4, 5, colors[4]);
  matrixLine(5, 6, colors[3]);
  matrixLine(6, 7, colors[2]);
  matrixLine(7, 8, colors[1]);
  matrix.show();
  delay(2000);
}

// Show electricity prices on the LED matrix
void matrixShowEntsoe() {
  matrix.fillScreen(0); // Clear the matrix
  
  Serial.println("Updating LED Matrix with price data...");
  
  int currentHour = getHoursOfDay();
  int firstDisplayHour = currentHour % 24; // Which hour the first column represents
  
  for (int i = 0; i < 8; i++) {
    if (!PRICES.price[i].isNull) {
      int height;
      
      // Calculate the height of the bar (1-8 pixels)
      if (PRICES.maximumPrice == PRICES.minimumPrice) {
        height = 4; // Middle height if all prices equal
      } else {
        height = (int)(7 * (PRICES.price[i].price - PRICES.minimumPrice) / 
                      (PRICES.maximumPrice - PRICES.minimumPrice)) + 1;
      }

      // Constrain height to matrix bounds
      if (height < 1) height = 1;
      if (height > 8) height = 8;

      // Get color based on price level
      int level = PRICES.price[i].level;
      if (level < 0) level = 0;
      if (level > 4) level = 4;

      // Draw the column
      matrixLine(i, height, colors[level]);
      
      Serial.printf("  Col %d: price=%d, height=%d, level=%d\n", 
                    i, PRICES.price[i].price, height, level);
    } else {
      // No data for this hour - draw a dim dot at bottom
      drawPixelRemapped(i, 7, matrix.Color(10, 10, 10)); // Very dim
    }
  }

  // Flash the first column (current hour) - on/off alternating
  if (!PRICES.price[0].isNull) {
    int animHeight;
    if (PRICES.maximumPrice == PRICES.minimumPrice) {
      animHeight = 4;
    } else {
      animHeight = (int)(7 * (PRICES.price[0].price - PRICES.minimumPrice) / 
                        (PRICES.maximumPrice - PRICES.minimumPrice)) + 1;
    }
    if (animHeight < 1) animHeight = 1;
    if (animHeight > 8) animHeight = 8;
    
    if (blinkState) {
      // Show normally (already drawn above)
    } else {
      // Hide current hour column (overwrite with empty)
      for (int y = 0; y < animHeight; y++) {
        drawPixelRemapped(0, 7 - y, matrix.Color(0, 0, 0));
      }
    }
    blinkState = !blinkState;
  } else {
    blinkState = false;
  }

  matrix.show(); // Update the matrix display
}

// Show a simple animation when WiFi is connecting / portal is active
void matrixShowConnecting() {
  static int step = 0;
  matrix.fillScreen(0);
  
  // Draw a spinning pattern
  int x = step % 8;
  drawPixelRemapped(x, 0, matrix.Color(0, 0, 255));
  drawPixelRemapped(7 - x, 7, matrix.Color(0, 0, 255));
  
  matrix.show();
  step = (step + 1) % 8;
  delay(100);
}

// Show AP mode indicator - alternating blue 'A' pattern on LED matrix
// Visual: two vertical blue bars that alternate left/right to indicate config mode
void matrixShowAPMode() {
  static unsigned long lastToggle = 0;
  static bool apState = false;
  
  unsigned long now = millis();
  
  if (now - lastToggle > 500) { // Toggle every 500ms
    lastToggle = now;
    apState = !apState;
    matrix.fillScreen(0);
    
    if (apState) {
      // Pattern 1: Left column + right column blue (brackets)
      for (int y = 0; y < 8; y++) {
        drawPixelRemapped(0, y, matrix.Color(0, 0, 80));  // Dim blue left
        drawPixelRemapped(7, y, matrix.Color(0, 0, 80));  // Dim blue right
      }
      // Center 4x4 block
      for (int x = 2; x <= 5; x++) {
        for (int y = 2; y <= 5; y++) {
          drawPixelRemapped(x, y, matrix.Color(0, 0, 120));  // Medium blue center
        }
      }
    } else {
      // Pattern 2: Show "AP" hint - top and bottom rows blue
      for (int x = 0; x < 8; x++) {
        drawPixelRemapped(x, 0, matrix.Color(0, 0, 60));   // Dim blue top
        drawPixelRemapped(x, 7, matrix.Color(0, 0, 60));   // Dim blue bottom
      }
    }
    
    matrix.show();
  }
}

// Non-blocking version - call this in loop() without delay
// Set to true when AP mode is active
bool apModeActive = false;

void matrixUpdateAPMode() {
  if (apModeActive) {
    matrixShowAPMode();
  }
}

// Show an error pattern (all red flash)
void matrixShowError() {
  matrix.fillScreen(0);
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      drawPixelRemapped(i, j, matrix.Color(255, 0, 0));
    }
  }
  matrix.show();
  delay(500);
  matrix.fillScreen(0);
  matrix.show();
  delay(500);
}

#endif // HELPER_LED_H
