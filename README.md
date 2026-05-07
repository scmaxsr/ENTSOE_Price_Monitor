# ENTSO-E Prijs Monitor

Klein project om de dynamische uurlijkse elektriciteitsprijzen van de **ENTSO-E Transparency Platform** (Europese marktkoppeling) weer te geven op een eenvoudige 8x8 LED matrix.

De eerste kolom is het huidige uur, de andere 7 kolommen tonen de komende 7 uren.
De hoogte van de balk geeft de relatieve prijs weer (t.o.v. min/max in de komende 8 uur).
De kleuren geven het prijsniveau aan (zelf berekend op basis van de range).

**LET OP:** Dit is de *marktprijs* (day-ahead), niet je uiteindelijke leveringstarief. De werkelijke prijs die je betaalt is marktprijs + belastingen + transportkosten + leveranciersopslag.

## 🆚 Verschil met Tibber-versie

| Aspect | Tibber (origineel) | ENTSO-E (deze versie) |
|--------|-------------------|----------------------|
| Databron | Tibber API (betaald account) | ENTSO-E (gratis, open data) |
| API kosten | Alleen met Tibber abonnement | Gratis API key |
| Configuratie | Hardcoded in settings.h | **WiFi Configuratie Portal** |
| Data scope | Jouw specifieke contractprijs | Day-ahead marktprijs NL |
| Prijsniveaus | Vanuit Tibber API (5 levels) | Zelf berekend o.b.v. min/max |

## 🚀 Installatie-instructies

### Hardware
- **ESP8266 bord** (Wemos D1 Mini / AZDelivery D1 Mini)
- **8x8 NeoPixel LED Matrix** (bijv. AZDelivery 8x8 LED Matrix)
- **USB-kabel** voor stroom en uploaden

### Aansluiten
| LED Matrix | Wemos D1 Mini |
|------------|---------------|
| VCC / 5V   | 5V (of VU)   |
| GND        | GND           |
| DIN / Data | D3 (GPIO0)    |

### Software installatie

1. **ENTSO-E API Key aanvragen** (gratis):
   - Ga naar https://transparency.entsoe.eu
   - Registreer een account
   - Ga naar "My Account" → "API Key"
   - Genereer een nieuwe API key

2. **Arduino IDE instellen**:
   - Installeer de **ESP8266 board** package (Boards Manager → "esp8266 by ESP8266 Community")
   - Installeer de volgende libraries (Sketch → Include Library → Manage Libraries):
     - `Adafruit NeoPixel` (by Adafruit)
     - `Adafruit GFX Library` (by Adafruit)
     - `Adafruit NeoMatrix` (by Adafruit)

3. **Open het project**:
   - Open `ENTSOE_Price_Monitor.ino` in de Arduino IDE
   - Selecteer je bord: **LOLIN(WEMOS) D1 R2 & mini**
   - Upload de sketch

4. **Eerste configuratie via WiFi Portal**:
   - Na het uploaden start de ESP een WiFi access point: **ENTSOE-Monitor-Config**
   - Verbind met dit netwerk (geen wachtwoord)
   - Er opent automatisch een configuratiescherm (captive portal)
   - Of navigeer handmatig naar http://192.168.4.1
   - Vul in:
     - 🏠 **WiFi SSID** + wachtwoord
     - 🔑 **ENTSO-E API Key** (je persoonlijke key)
     - 🌍 **Bidding Zone** (standaard: `10YNL----------L` voor Nederland)
   - Klik op "Opslaan & Verbinden"
   - Het apparaat herstart en verbindt met je WiFi

5. **Configuratie wijzigen?** 
   - Houd **D3 (GPIO0)** laag tijdens opstarten of
   - Gebruik een paperclip om de configuratie te resetten (of her-upload de sketch)

## 📊 Werking

- Het apparaat haalt eenmaal per uur de day-ahead prijzen op van ENTSO-E
- De prijzen worden weergegeven op een 8x8 LED matrix:
  - **8 kolommen** = huidig uur + 7 uur vooruit
  - **Hoogte** = relatieve prijs (t.o.v. min/max)
  - **Kleur** = prijsniveau:
    - 🟢 Groen = Zeer goedkoop
    - 🟢 Licht Groen = Goedkoop
    - 🟡 Oranje/Geel = Normaal
    - 🟠 Oranje = Duur
    - 🔴 Rood = Zeer duur
  - **Eerste kolom knippert** = huidige uur

## 📝 Enkele notities

- De ENTSO-E dagvooruit-prijzen worden meestal rond **12:00-13:00** gepubliceerd
- Voor uren zonder data blijft de kolom donker (alleen een klein puntje)
- WiFi wordt alleen ingeschakeld tijdens data ophalen (energiebesparing)
- De ESP8266 heeft ~40-80KB vrije heap — de XML parsing is geoptimaliseerd voor laag geheugengebruik

## 🔧 Hardware gebruikt

- **AZDelivery 8x8 LED Matrix** - https://www.amazon.nl/dp/B078HYP681
- **AZDelivery D1 Mini (ESP8266)** - https://www.amazon.nl/-/en/dp/B01N9RXGHY

Hardware kosten: ~€10-15

## 📦 3D-geprinte behuizing

Deze behuizing van Hatrick3D past ook bij dit project:
[https://www.printables.com/de/model/908102-8x8-led-matrix-display-as-energy-price-indicator-f](https://www.printables.com/de/model/908102-8x8-led-matrix-display-as-energy-price-indicator-f)

## 📸 Eindresultaat
![LED display met 8 kolommen](images/picture.jpeg)

## ⚙️ Voor ontwikkelaars

### Bestandsstructuur
```
ENTSOE_Price_Monitor/
├── ENTSOE_Price_Monitor.ino    # Hoofdprogramma
├── settings.h                   # Standaard instellingen
├── helper_wifi_portal.h         # WiFi configuratie portal (eerste setup)
├── helper_time.h                # NTP tijd synchronisatie
├── helper_memory.h              # Geheugen debugging
├── helper_entsoe.h              # ENTSO-E API + XML parsing
├── helper_led.h                 # 8x8 LED matrix aansturing
└── README.md
```

### ENTSO-E API
- Endpoint: `https://web-api.tp.entsoe.eu/api`
- Document type: `A44` (day-ahead prices)
- Bidding zone NL: `10YNL----------L`
- Response: XML met `price.amount` en `position` per uur
