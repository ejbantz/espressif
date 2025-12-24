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
- **API Endpoint**: `https://ejdevhelpcenter-developer-edition.develop.my.site.com/vforcesite/services/apexrest/sensor/reading`

### SensorDataAPI

REST endpoint for sensor readings.

**POST** - Create reading:
```json
{
  "temperature": 72.5,
  "humidity": 45.0,
  "deviceId": "ESP32-001"
}
```

**Headers required:**
- `X-API-Key: LawnMonitor2024SecretKey`
- `Content-Type: application/json`

### Objects

- **Sensor_Reading__c**: Stores sensor data (Temperature__c, Humidity__c, Device_Id__c, Reading_Timestamp__c)

## Setup

1. Copy credentials template: `cp include/credentials.h.example include/credentials.h`
2. Edit credentials with your WiFi info
3. Connect ESP32 via USB
4. Run `pio run --target upload`

## Salesforce Deployment

```bash
sf project deploy start --source-dir force-app -o agentforce
```
