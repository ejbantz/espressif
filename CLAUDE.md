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
- **Temperature**: MCP9808 I2C sensor (on SIM7000A shield)
- **SIM**: Hologram IoT SIM

### SIM7000A Wiring

| SIM7000A | ESP32 | Notes |
|----------|-------|-------|
| D6 (PWRKEY) | GPIO26 | Modem power control |
| D10 (TX) | GPIO16 | Modem TX → ESP32 RX |
| D11 (RX) | GPIO17 | ESP32 TX → Modem RX |
| 5V/LOGIC | 5V | Charges LiPo when USB connected |
| GND | GND | Ground |

**Power**: LiPo battery connected to SIM7000A JST connector powers both modem and ESP32 (via HT7333 LDO regulator to ESP32 3.3V pin). ESP32 5V pin → Shield 5V/LOGIC enables battery charging when USB connected.

**Baud**: Modem communicates at 57600 baud.

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

## OTA Firmware Updates

The device supports over-the-air firmware updates from Salesforce. User triggers update by 5-tap on the boot button.

### Button Commands

- **1-tap**: Send sensor reading
- **2-tap**: Send sensor reading
- **3-tap**: Scan WiFi networks
- **4-tap**: Toggle BLE (wait for timeout)
- **5-tap**: Check for firmware update (immediate)

### Firmware Version

Current version is defined in `src/main.cpp`:
```cpp
const char* FIRMWARE_VERSION = "1.0.0";
```

### Salesforce Components

- **FirmwareAPI**: REST endpoint at `/sensor/firmware` returns version and download URL
- **Firmware_Config__mdt**: Custom metadata stores current version number
- **ESP32_Firmware**: Static resource contains the firmware.bin file

### Releasing a New Firmware Version

1. **Update version** in `src/main.cpp`:
   ```cpp
   const char* FIRMWARE_VERSION = "1.0.1";
   ```

2. **Build firmware**:
   ```bash
   pio run
   ```

3. **Copy to static resources**:
   ```bash
   cp .pio/build/esp32dev/firmware.bin force-app/main/default/staticresources/ESP32_Firmware.bin
   ```

4. **Deploy to Salesforce**:
   ```bash
   sf project deploy start --source-dir force-app/main/default/staticresources -o agentforce
   ```

5. **Update version in Salesforce**:
   - Setup → Custom Metadata Types → Firmware Config → Manage Records
   - Edit "ESP32_Sensor" record
   - Change Firmware_Version__c to match (e.g., "1.0.1")
   - Save

### Device Update Process

When user 5-taps:
1. Device calls `/sensor/firmware?apiKey=...` to get current version
2. Compares server version to local `FIRMWARE_VERSION`
3. If newer, downloads .bin from static resource URL
4. Flashes firmware and reboots

### OTA via Local Network

For development, ArduinoOTA is also enabled:
```bash
pio run -t upload -e esp32dev_ota
```
Requires ESP32 on same network. IP configured in `platformio.ini`.
