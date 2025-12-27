# ESP32 Salesforce IoT Project

ESP32 microcontroller that pushes sensor data to Salesforce via REST API.

## Project Structure

```
espressif/
├── src/main.cpp           # ESP32 Arduino code
├── include/credentials.h  # WiFi credentials (gitignored)
├── platformio.ini         # PlatformIO config
├── force-app/             # Salesforce metadata
│   └── main/default/
│       └── classes/       # Apex classes including SensorDataAPI
└── config/                # SFDX scratch org config
```

## Hardware

- **Board**: ESP32-DevKitC-32E
- **Chip**: ESP32-WROOM-32E (dual-core 240MHz, WiFi + BT)
- **USB**: CP210x USB-to-serial
- **Cellular**: Botletics SIM7000A (LTE CAT-M1/NB-IoT + GPS)
- **SIM**: Hologram IoT SIM

### SIM7000A Wiring

| SIM7000A | ESP32 | Notes |
|----------|-------|-------|
| TX | GPIO16 | Modem TX → ESP32 RX |
| RX | GPIO17 | ESP32 TX → Modem RX |
| VIN | 5V | Power |
| GND | GND | Ground |
| PWRKEY | GPIO26 | Power control |

### GPIO Pin Usage

- GPIO0: Boot button (multi-tap)
- GPIO4: Touch sensor
- GPIO16: Modem RX (Serial2)
- GPIO17: Modem TX (Serial2)
- GPIO25: Buzzer (PWM)
- GPIO26: Modem PWRKEY
- GPIO34: Soil moisture sensor (ADC)

## ESP32 Development

**Build and upload:**
```bash
pio run --target upload
```

**Monitor serial output:**
```bash
pio device monitor
```

**WiFi credentials** are stored in `include/credentials.h` (not committed):
```cpp
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";
```

## Salesforce Org

- **Alias**: agentforce
- **Site**: EJ_Dev (path: vforcesite)
- **API Endpoint**: `https://ejdev-dev-ed.develop.my.site.com/vforcesite/services/apexrest/sensor/reading`

### SensorDataAPI

REST endpoint for sensor readings.

**POST** - Create reading:
```json
{
  "temperature": 72.5,
  "humidity": 45.0,
  "deviceId": "ESP32-001",
  "function": "Single",
  "connectionType": "WiFi",
  "latitude": 37.774929,
  "longitude": -122.419418,
  "apiKey": "LawnMonitor2024SecretKey"
}
```

### Objects

- **Sensor_Reading__c** fields:
  - Temperature__c, Humidity__c
  - Device_Id__c, Function__c
  - Latitude__c, Longitude__c
  - Connection_Type__c (Phone/WiFi/Cellular)
  - Reading_Timestamp__c

## Connection Priority

Data is sent using the first available method:
1. **Phone** - BLE relay to iOS app (requires 4-tap to enable BLE)
2. **WiFi** - Direct HTTP POST
3. **Cellular** - LTE via SIM7000A (fallback)

## Setup

1. Copy credentials template: `cp include/credentials.h.example include/credentials.h`
2. Edit credentials with your WiFi info
3. Connect ESP32 via USB
4. Run `pio run --target upload`

## Salesforce Deployment

```bash
sf project deploy start --source-dir force-app -o agentforce
```
