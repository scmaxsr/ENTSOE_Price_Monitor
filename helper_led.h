#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// MATRIX DECLARATION:
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, D3,
  NEO_MATRIX_BOTTOM     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

const uint16_t colors[] = {
  matrix.Color(0, 255, 0), matrix.Color(85, 170, 0), matrix.Color(105, 140, 0), matrix.Color(128, 128, 0), matrix.Color(255, 0, 0) };

bool firstBlink;

void matrixInitialize() {
  Serial.println("Initialize Matrix");
  matrix.begin();
  matrix.setBrightness(20);
  firstBlink = true;
}

// Function to remap pixels with horizontal and vertical flipping
void drawPixelRemapped(int x, int y, uint16_t color) {
  // Flip the x-coordinate for horizontal reversal
  x = 7 - x; // Assuming an 8x8 matrix
  // Flip the y-coordinate for vertical reversal
  if (x % 2 == 0) {
    y = 7 - y; // Even columns
  }
  matrix.drawPixel(x, y, color);
}

void matrixLine(int column, int height, uint16_t color) {
  for (int y = 0; y < height; y++) {
    drawPixelRemapped(column, 7 - y, color); // Start from the bottom and grow upwards
  }
}

void matrixShow() {
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
}

void matrixShowTibber() {
  matrix.fillScreen(0); // Clear the matrix
  firstBlink = true; // Toggle blinking state
  Serial.println("Calculating LEDs");

  for (int i = 0; i < 8; i++) {
    if (!PRICES.price[i].isNull) {
      int height;
      
      // Calculate the height of the bar
      if (PRICES.maximumPrice == PRICES.minimumPrice) {
        height = 1; // Avoid division by zero
      } else {
        height = (int)(8 * (PRICES.price[i].price - PRICES.minimumPrice) / 
                      (PRICES.maximumPrice - PRICES.minimumPrice)) + 1;
      }

      // Constrain height to matrix bounds
      if (height > 8) {
        height = 8;
      }

      // Debugging output
      Serial.println(PRICES.price[i].price);
      Serial.println(PRICES.price[i].level);
      Serial.println(i);
      Serial.println(height);

      // Draw the column if conditions are met
      if (i != 0 || firstBlink) {
        matrixLine(i, height, colors[PRICES.price[i].level]);
      }
    }
  }

  matrix.show(); // Update the matrix display
}
