# ENTSO-E Price Monitor

ESP8266 (Wemos D1 Mini) with 8x8 LED Matrix that displays day-ahead electricity market prices from the ENTSO-E Transparency Platform.

## How it works

Every hour, the device fetches day-ahead electricity prices for the next 8 hours from ENTSO-E and displays them as a bar chart on the LED matrix:

- **8 columns**: current hour + next 7 hours
- **Bar height**: relative price (higher = more expensive)
- **Current hour**: blinking column
- **Color coding**: 
  - 🟢 Green: very cheap (level 1)
  - 🔵 Blue: cheap (level 2)
  - 🟡 Yellow: normal (level 3)
  - 🟠 Orange: expensive (level 4)
  - 🔴 Red: very expensive (level 5)

On first boot, the device creates a WiFi access point with a configuration portal (LED matrix shows blue blinking pattern ⚡). After setup, it runs standalone and only connects to WiFi once per hour to fetch new prices, saving power. If the device cannot find a saved configuration, it automatically enters AP mode and the LED matrix displays a blue animation to indicate it's ready for setup.

## Hardware

| Component | Type |
|-----------|------|
| **Board** | Wemos D1 Mini (ESP8266) |
| **Display** | 8x8 NeoPixel LED Matrix (WS2812) |
| **Power** | 5V USB (≥1A recommended) |

### Wiring

| Wemos D1 Mini | LED Matrix |
|:---|:---|
| 5V | VCC / 5V |
| GND | GND |
| D4 (GPIO2) | DIN / Data In |

> **Note:** If your matrix has 3 pins (VCC, GND, DIN), you only need 3 wires. If it has 4 pins (also includes DOUT), leave DOUT unconnected.

## Installation

### 1. Get an ENTSO-E API Key

1. Go to [ENTSO-E Transparency Platform](https://transparency.entsoe.eu)
2. Create an account (free)
3. Navigate to: My Profile → Web Security → API Key
4. Request a key (approval usually takes 1 business day)

### 2. Install Arduino Libraries

Open the Arduino IDE Library Manager (Sketch → Include Library → Manage Libraries) and install:

- **Adafruit NeoPixel** (by Adafruit)
- **Adafruit GFX** (by Adafruit)
- **Adafruit NeoMatrix** (by Adafruit)

### 3. Upload the Sketch

1. Open `ENTSOE_Price_Monitor.ino` in Arduino IDE
2. Select board: **LOLIN(WEMOS) D1 R2 & mini**  
   (Tools → Board → ESP8266 Boards → LOLIN(WEMOS) D1 R2 & mini)
3. Select the correct COM port
4. Click Upload

### 4. Configure via WiFi

After flashing, the device starts in configuration mode:

1. Connect to WiFi network **`ENTSOE-Monitor-Config`** (no password)
2. A captive portal should open automatically. If not, open a browser and go to **`http://192.168.4.1`**
3. Enter:
   - **WiFi SSID & password** (your home network)
   - **ENTSO-E API key**
   - **Bidding zone** (default: `10YNL----------L` for Netherlands)
4. Click Save — the device reboots and starts displaying prices

> On subsequent boots, saved settings are loaded from internal flash memory. To reconfigure, hold GPIO0 low during boot (or manually erase SPIFFS).

## LED Matrix Visual Guide

| State | LED Matrix | Description |
|:------|:-----------|:------------|
| ⚪ Boot | White test pattern | All LEDs light up briefly at startup |
| ⚙️ Config Mode | **Blue blinking** 🔵 | Access point `ENTSOE-Monitor-Config` is active |
| 🌐 Connecting | Sweeping animation | Device is connecting to your WiFi network |
| 📊 Normal | Colored bar chart (8 columns) | Current hour (blinking) + next 7 hours |
| 🔴 Red column | Red bar | Very expensive (level 5) |
| 🟢 Green column | Green bar | Very cheap (level 1) |

## Bidding Zones

| Country | Zone Code |
|:--------|:----------|
| 🇳🇱 Netherlands | `10YNL----------L` |
| 🇧🇪 Belgium | `10YBE----------2` |
| 🇩🇪 Germany | `10YDE-VIE-------` |
| 🇫🇷 France | `10YFR----------8` |
| 🇬🇧 UK | `10YGB----------A` |

## Troubleshooting

| Problem | Solution |
|:--------|:---------|
| LED matrix stays dark | Check wiring: VCC→5V, GND→GND, DIN→D4 |
| "Failed to obtain time" | Device needs internet access via WiFi |
| No prices shown | Verify your API key in the config portal |
| Config portal won't open | Reset device, or manually erase flash via serial |

## License

This project is licensed under the **GNU General Public License v3.0**.
Based on the [Tibber_Price_Monitor](https://github.com/ardboer/Tibber_Price_Monitor) by ardboer.
