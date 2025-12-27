#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include "esp_coexist.h"
#include "credentials.h"

// BLE Configuration
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BUTTON_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATUS_CHAR_UUID    "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define WIFI_SCAN_CHAR_UUID "a1e8f5d2-7b3c-4e9a-8f2d-6c5b4a3d2e1f"
#define WIFI_CRED_CHAR_UUID "b2f9e6c3-8c4d-5f0b-9e3e-7d6c5b4a3f20"
#define WIFI_STATUS_CHAR_UUID "c3a0f7d4-9d5e-6f1c-0a4f-8e7d6c5b4a31"
#define SENSOR_CHAR_UUID "d4b1e8f5-0e6f-7a2d-1b5a-9f8e7d6c5b42"

// Buzzer on GPIO25
const int BUZZER_PIN = 25;

// Preferences for storing WiFi credentials
Preferences preferences;
#define MAX_SAVED_NETWORKS 5

BLEServer* pServer = NULL;
BLECharacteristic* pButtonChar = NULL;
BLECharacteristic* pStatusChar = NULL;
BLECharacteristic* pWifiScanChar = NULL;
BLECharacteristic* pWifiCredChar = NULL;
BLECharacteristic* pWifiStatusChar = NULL;
BLECharacteristic* pSensorChar = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool wifiScanRequested = false;
bool newCredentialsReceived = false;
bool forgetNetworkRequested = false;
bool pauseSensorUpdates = false;  // Pause during HTTP requests
String newSSID = "";
String newPassword = "";
String forgetSSID = "";

void beepBleConnect();  // Forward declaration
void beepBleDisconnect();
void beepWifiConnect();
void beepFail();

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE: Phone connected");
        beepBleConnect();
    };
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE: Phone disconnected");
        beepBleDisconnect();
    }
};

// Callback for WiFi scan requests
class WifiScanCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue().c_str();
        if (value == "SCAN") {
            Serial.println("BLE: WiFi scan requested");
            wifiScanRequested = true;
        }
    }
};

// Callback for receiving WiFi credentials
class WifiCredCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue().c_str();
        Serial.print("BLE: Received: ");
        Serial.println(value);

        // Check for FORGET command: "FORGET:SSID"
        if (value.startsWith("FORGET:")) {
            forgetSSID = value.substring(7);
            forgetNetworkRequested = true;
            Serial.print("Forget requested for: ");
            Serial.println(forgetSSID);
            return;
        }

        // Format: "SSID:password" or "SSID:" for open networks
        int colonIndex = value.indexOf(':');
        if (colonIndex > 0) {
            newSSID = value.substring(0, colonIndex);
            newPassword = value.substring(colonIndex + 1);
            newCredentialsReceived = true;
            Serial.print("Parsed SSID: ");
            Serial.println(newSSID);
        }
    }
};

void notifyPhone(const char* status) {
    if (deviceConnected && pStatusChar) {
        pStatusChar->setValue(status);
        pStatusChar->notify();
    }
}

void notifyButtonState(bool pressed) {
    if (deviceConnected && pButtonChar) {
        pButtonChar->setValue(pressed ? "PRESSED" : "RELEASED");
        pButtonChar->notify();
    }
}

void updateWifiStatus() {
    if (pWifiStatusChar) {
        String status;
        if (WiFi.status() == WL_CONNECTED) {
            status = WiFi.SSID();
        } else {
            status = "Disconnected";
        }
        pWifiStatusChar->setValue(status.c_str());
        if (deviceConnected) {
            pWifiStatusChar->notify();
        }
    }
}

// Buzzer functions using PWM tones
#define BUZZER_CHANNEL 0

void playTone(int frequency, int duration) {
    ledcWriteTone(BUZZER_CHANNEL, frequency);
    delay(duration);
    ledcWriteTone(BUZZER_CHANNEL, 0);
}

void beep(int duration) {
    playTone(1000, duration);
}

void beepSuccess() {
    // Two quick high beeps (ding ding!)
    playTone(1200, 60);
    delay(40);
    playTone(1500, 80);
}

void beepFail() {
    // Low buzz-buzz (error)
    playTone(200, 150);
    delay(80);
    playTone(150, 200);
}

void beepTap(int count) {
    // Short click
    playTone(800, 15);
}

void beepBleConnect() {
    // Rising arpeggio - phone connected (C-E-G-C)
    playTone(523, 60);   // C5
    delay(30);
    playTone(659, 60);   // E5
    delay(30);
    playTone(784, 60);   // G5
    delay(30);
    playTone(1047, 100); // C6
}

void beepBleDisconnect() {
    // Descending two-tone - phone disconnected
    playTone(784, 80);   // G5
    delay(40);
    playTone(440, 120);  // A4
}

void beepWifiConnect() {
    // Single warm tone - WiFi connected
    playTone(660, 200);  // E5
}

void beepWifiDisconnect() {
    // Low single tone - WiFi lost
    playTone(330, 200);  // E4
}

void beepStartup() {
    // Three ascending notes - boot (like Mac)
    playTone(440, 80);   // A4
    delay(50);
    playTone(554, 80);   // C#5
    delay(50);
    playTone(659, 150);  // E5
}

void beepScanning() {
    // Quick chirp - scanning
    playTone(1000, 30);
    delay(50);
    playTone(1200, 30);
}

// WiFi credential storage functions
void saveWifiCredential(const String& ssid, const String& password) {
    preferences.begin("wifi", false);

    // Find empty slot or existing entry
    for (int i = 0; i < MAX_SAVED_NETWORKS; i++) {
        String key = "ssid" + String(i);
        String existingSSID = preferences.getString(key.c_str(), "");

        if (existingSSID == "" || existingSSID == ssid) {
            preferences.putString(key.c_str(), ssid);
            preferences.putString(("pass" + String(i)).c_str(), password);
            Serial.print("Saved WiFi credentials for: ");
            Serial.println(ssid);
            preferences.end();
            return;
        }
    }

    // No empty slot, overwrite oldest (slot 0) and shift others
    for (int i = 0; i < MAX_SAVED_NETWORKS - 1; i++) {
        String nextSSID = preferences.getString(("ssid" + String(i + 1)).c_str(), "");
        String nextPass = preferences.getString(("pass" + String(i + 1)).c_str(), "");
        preferences.putString(("ssid" + String(i)).c_str(), nextSSID);
        preferences.putString(("pass" + String(i)).c_str(), nextPass);
    }
    preferences.putString(("ssid" + String(MAX_SAVED_NETWORKS - 1)).c_str(), ssid);
    preferences.putString(("pass" + String(MAX_SAVED_NETWORKS - 1)).c_str(), password);
    Serial.print("Saved WiFi credentials (shifted): ");
    Serial.println(ssid);

    preferences.end();
}

String getSavedPassword(const String& ssid) {
    preferences.begin("wifi", true);

    for (int i = 0; i < MAX_SAVED_NETWORKS; i++) {
        String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (savedSSID == ssid) {
            String password = preferences.getString(("pass" + String(i)).c_str(), "");
            preferences.end();
            return password;
        }
    }

    preferences.end();
    return "";
}

void listSavedNetworks() {
    preferences.begin("wifi", true);
    Serial.println("Saved networks:");
    for (int i = 0; i < MAX_SAVED_NETWORKS; i++) {
        String ssid = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (ssid != "") {
            Serial.print("  ");
            Serial.println(ssid);
        }
    }
    preferences.end();
}

void forgetWifiNetwork(const String& ssid) {
    preferences.begin("wifi", false);

    int foundIndex = -1;
    for (int i = 0; i < MAX_SAVED_NETWORKS; i++) {
        String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (savedSSID == ssid) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex == -1) {
        Serial.print("Network not found: ");
        Serial.println(ssid);
        preferences.end();
        return;
    }

    // Shift remaining networks down to fill the gap
    for (int i = foundIndex; i < MAX_SAVED_NETWORKS - 1; i++) {
        String nextSSID = preferences.getString(("ssid" + String(i + 1)).c_str(), "");
        String nextPass = preferences.getString(("pass" + String(i + 1)).c_str(), "");
        preferences.putString(("ssid" + String(i)).c_str(), nextSSID);
        preferences.putString(("pass" + String(i)).c_str(), nextPass);
    }

    // Clear the last slot
    preferences.putString(("ssid" + String(MAX_SAVED_NETWORKS - 1)).c_str(), "");
    preferences.putString(("pass" + String(MAX_SAVED_NETWORKS - 1)).c_str(), "");

    Serial.print("Forgot network: ");
    Serial.println(ssid);
    preferences.end();
}

void performWifiScanForPhone() {
    Serial.println("Performing WiFi scan for phone...");
    notifyPhone("Scanning...");

    int numNetworks = WiFi.scanNetworks();

    // Build JSON array of networks (limit to 5 to fit BLE MTU)
    String result = "[";
    int count = 0;
    for (int i = 0; i < numNetworks && count < 5; i++) {
        String ssid = WiFi.SSID(i);
        // Skip empty SSIDs
        if (ssid.length() == 0) continue;

        if (count > 0) result += ",";
        result += "{\"ssid\":\"";
        result += ssid;
        result += "\",\"rssi\":";
        result += WiFi.RSSI(i);
        result += ",\"open\":";
        result += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "true" : "false";

        // Check if we have saved credentials for this network
        String savedPass = getSavedPassword(ssid);
        result += ",\"saved\":";
        result += (savedPass != "") ? "true" : "false";
        result += "}";
        count++;
    }
    result += "]";

    Serial.print("WiFi JSON (");
    Serial.print(result.length());
    Serial.print(" bytes): ");
    Serial.println(result);

    WiFi.scanDelete();

    // Send to phone via BLE (may need to chunk for large results)
    if (pWifiScanChar) {
        pWifiScanChar->setValue(result.c_str());
        pWifiScanChar->notify();
    }

    Serial.print("Sent ");
    Serial.print(numNetworks);
    Serial.println(" networks to phone");
    notifyPhone("Scan complete");
}

// Salesforce API Configuration
const char* SF_ENDPOINT = "https://ejdev-dev-ed.develop.my.site.com/vforcesite/services/apexrest/sensor/reading";
const char* SF_API_KEY = "LawnMonitor2024SecretKey";
const char* DEVICE_ID = "ESP32-001";

// Boot button on GPIO0
const int BUTTON_PIN = 0;

// Touch sensor on GPIO4 (T0)
const int TOUCH_PIN = T0;  // GPIO4
const int TOUCH_THRESHOLD = 40;  // Values below this = touched

// Soil moisture sensor on GPIO34 (ADC1)
const int MOISTURE_PIN = 34;
const int MOISTURE_DRY = 2800;   // ADC value when dry (in air)
const int MOISTURE_WET = 1200;   // ADC value when wet (in water)

// Multi-tap timing (ms)
const unsigned long TAP_WINDOW = 400;
const unsigned long DEBOUNCE_TIME = 50;

WiFiClientSecure client;

String connectedSSID = "";

bool tryConnect(const char* ssid, const char* password = NULL) {
    Serial.print("Trying: ");
    Serial.println(ssid);

    if (password) {
        WiFi.begin(ssid, password);
    } else {
        WiFi.begin(ssid);
    }

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    return WiFi.status() == WL_CONNECTED;
}

void postConnectionStatus(const char* ssid, bool isOpen) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    client.setInsecure();
    http.begin(client, SF_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"function\":\"WiFi Connect\",";
    payload += "\"networks\":\"" + String(ssid) + (isOpen ? " (Open)" : " (Private)") + "\",";
    payload += "\"apiKey\":\"" + String(SF_API_KEY) + "\"";
    payload += "}";

    Serial.println("Posting connection status to Salesforce...");
    int httpCode = http.POST(payload);
    if (httpCode > 0) {
        Serial.print("Posted: ");
        Serial.println(http.getString());
    }
    http.end();
}

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("\n--- Scanning for networks ---");
    listSavedNetworks();

    int numNetworks = WiFi.scanNetworks();

    // Priority 1: Try saved networks that are visible
    Serial.println("Checking for saved networks...");
    preferences.begin("wifi", true);
    for (int i = 0; i < MAX_SAVED_NETWORKS; i++) {
        String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (savedSSID == "") continue;

        // Check if this saved network is visible
        for (int j = 0; j < numNetworks; j++) {
            if (WiFi.SSID(j) == savedSSID) {
                String savedPass = preferences.getString(("pass" + String(i)).c_str(), "");
                preferences.end();

                Serial.print("Found saved network: ");
                Serial.println(savedSSID);

                if (tryConnect(savedSSID.c_str(), savedPass.length() > 0 ? savedPass.c_str() : NULL)) {
                    connectedSSID = savedSSID;
                    Serial.println("Connected to saved network!");
                    Serial.print("IP Address: ");
                    Serial.println(WiFi.localIP());
                    WiFi.scanDelete();
                    postConnectionStatus(savedSSID.c_str(), false);
                    beepWifiConnect();
                    updateWifiStatus();
                    return;
                }
                preferences.begin("wifi", true);
                break;
            }
        }
    }
    preferences.end();

    // Priority 2: Try open networks
    Serial.println("Checking for open networks...");
    for (int i = 0; i < numNetworks; i++) {
        if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
            String ssid = WiFi.SSID(i);
            Serial.print("Found open network: ");
            Serial.print(ssid);
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.println(" dBm)");

            if (tryConnect(ssid.c_str(), NULL)) {
                connectedSSID = ssid;
                Serial.println("Connected to open network!");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
                WiFi.scanDelete();
                postConnectionStatus(ssid.c_str(), true);
                beepWifiConnect();
                updateWifiStatus();
                return;
            }
        }
    }

    WiFi.scanDelete();

    // Priority 3: Fall back to hardcoded credentials
    Serial.println("No saved/open networks available, using default WiFi...");
    if (tryConnect(WIFI_SSID, WIFI_PASSWORD)) {
        connectedSSID = WIFI_SSID;
        Serial.println("WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        postConnectionStatus(WIFI_SSID, false);
        beepWifiConnect();
    } else {
        Serial.println("WiFi Connection FAILED");
        beepFail();
    }
    updateWifiStatus();
}

bool sendToSalesforce(float temperature, float humidity, const char* function) {
    // Stop BLE completely to free the radio for WiFi
    bool wasConnected = deviceConnected;
    Serial.println("Pausing BLE for HTTP...");
    BLEDevice::stopAdvertising();
    if (wasConnected && pServer) {
        pServer->disconnect(pServer->getConnId());
    }
    delay(500);  // Wait for BLE to fully stop

    // Reconnect WiFi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi dropped, reconnecting...");
        WiFi.reconnect();
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(250);
            attempts++;
        }
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi reconnect failed");
            BLEDevice::startAdvertising();
            return false;
        }
        Serial.println("WiFi reconnected");
    }

    // Log WiFi status
    Serial.print("WiFi RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.print(" dBm, IP: ");
    Serial.println(WiFi.localIP());

    // Use fresh client for each request to avoid stale SSL state
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    http.begin(secureClient, SF_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"function\":\"" + String(function) + "\",";
    payload += "\"apiKey\":\"" + String(SF_API_KEY) + "\"";
    payload += "}";

    Serial.println("Sending to Salesforce...");
    Serial.println(payload);

    // Single attempt - BLE is stopped so should work
    int httpCode = http.POST(payload);
    bool success = false;

    if (httpCode > 0) {
        String response = http.getString();
        Serial.print("Response (");
        Serial.print(httpCode);
        Serial.print("): ");
        Serial.println(response);

        if (httpCode == 200 || httpCode == 201) {
            success = true;
        } else {
            Serial.print("SF Error: ");
            Serial.println(response);
        }
    } else {
        String errStr = http.errorToString(httpCode);
        Serial.print("HTTP Error: ");
        Serial.println(errStr);
    }

    http.end();

    // Resume BLE advertising
    Serial.println("Resuming BLE...");
    BLEDevice::startAdvertising();

    return success;
}

void setupBLE() {
    Serial.println("Starting BLE...");

    BLEDevice::init("ESP32-Sensor");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create service with more handles for additional characteristics
    BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID), 30);

    // Button state characteristic (notify)
    pButtonChar = pService->createCharacteristic(
        BUTTON_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pButtonChar->addDescriptor(new BLE2902());
    pButtonChar->setValue("RELEASED");

    // Status characteristic (notify) - shows what's being sent to Salesforce
    pStatusChar = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pStatusChar->addDescriptor(new BLE2902());
    pStatusChar->setValue("Ready");

    // WiFi scan characteristic - write "SCAN" to trigger, read/notify for results
    pWifiScanChar = pService->createCharacteristic(
        WIFI_SCAN_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
    );
    pWifiScanChar->addDescriptor(new BLE2902());
    pWifiScanChar->setCallbacks(new WifiScanCallbacks());
    pWifiScanChar->setValue("[]");

    // WiFi credentials characteristic - write "SSID:password" to save
    pWifiCredChar = pService->createCharacteristic(
        WIFI_CRED_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pWifiCredChar->setCallbacks(new WifiCredCallbacks());

    // WiFi status characteristic - shows current connection
    pWifiStatusChar = pService->createCharacteristic(
        WIFI_STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pWifiStatusChar->addDescriptor(new BLE2902());
    pWifiStatusChar->setValue("Disconnected");

    // Sensor reading characteristic - shows temp and moisture
    pSensorChar = pService->createCharacteristic(
        SENSOR_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pSensorChar->addDescriptor(new BLE2902());
    pSensorChar->setValue("--");

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE ready - look for 'ESP32-Sensor'");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 Salesforce IoT Device");
    Serial.println("================================");
    Serial.println("Single tap  = Send reading");
    Serial.println("Double tap  = Send reading");
    Serial.println("Triple tap  = Scan WiFi networks");
    Serial.println("Touch GPIO4 = Send reading");
    Serial.println("================================");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Setup buzzer PWM
    ledcSetup(BUZZER_CHANNEL, 2000, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);

    // Startup chime
    beepStartup();

    // Connect WiFi first (before BLE) so postConnectionStatus works
    connectWiFi();

    // Then start BLE
    setupBLE();
}

int readSoilMoisture() {
    // Read multiple samples and average for stability
    long total = 0;
    for (int i = 0; i < 10; i++) {
        total += analogRead(MOISTURE_PIN);
        delay(10);
    }
    return total / 10;
}

float getMoisturePercent(int rawValue) {
    // Convert raw ADC to percentage (0% = dry, 100% = wet)
    float percent = (float)(MOISTURE_DRY - rawValue) / (MOISTURE_DRY - MOISTURE_WET) * 100.0;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}

void sendReading(const char* function) {
    // Read internal chip temperature (Celsius) and convert to Fahrenheit
    float tempC = temperatureRead();
    float temp = (tempC * 9.0 / 5.0) + 32.0;

    // Read soil moisture sensor
    int moistureRaw = readSoilMoisture();
    float humidity = getMoisturePercent(moistureRaw);  // Using humidity field for moisture %

    Serial.println("\n--- Sending Sensor Data ---");
    Serial.print("Function: ");
    Serial.println(function);
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" F");
    Serial.print("Soil Moisture: ");
    Serial.print(humidity);
    Serial.print("% (raw: ");
    Serial.print(moistureRaw);
    Serial.println(")");

    // Update sensor characteristic
    char sensorMsg[50];
    snprintf(sensorMsg, sizeof(sensorMsg), "%.1fF | %.0f%%", temp, humidity);
    if (pSensorChar) {
        pSensorChar->setValue(sensorMsg);
        if (deviceConnected) {
            pSensorChar->notify();
        }
    }

    // Notify phone what we're sending
    char statusMsg[100];
    snprintf(statusMsg, sizeof(statusMsg), "Sending %s: %.1fF, Moisture %.0f%%", function, temp, humidity);
    notifyPhone(statusMsg);

    if (sendToSalesforce(temp, humidity, function)) {
        Serial.println("Success!");
        notifyPhone("Salesforce: Success!");
        beepSuccess();
    } else {
        Serial.println("Failed to send");
        notifyPhone("Salesforce: Failed!");
        beepFail();
    }
}

void scanAndSendNetworks() {
    Serial.println("\n--- Scanning WiFi Networks ---");

    // Stop BLE FIRST so WiFi scan has full radio access
    bool wasConnected = deviceConnected;
    Serial.println("Pausing BLE for scan + HTTP...");
    BLEDevice::stopAdvertising();
    if (wasConnected && pServer) {
        pServer->disconnect(pServer->getConnId());
    }
    delay(500);

    int numNetworks = WiFi.scanNetworks();
    Serial.print("Found ");
    Serial.print(numNetworks);
    Serial.println(" networks");

    char scanMsg[50];
    snprintf(scanMsg, sizeof(scanMsg), "Found %d networks", numNetworks);
    Serial.println(scanMsg);

    // Build networks JSON array
    String networks = "[";
    for (int i = 0; i < numNetworks && i < 20; i++) {  // Limit to 20 networks
        if (i > 0) networks += ",";
        networks += "{\"ssid\":\"";
        networks += WiFi.SSID(i);
        networks += "\",\"rssi\":";
        networks += WiFi.RSSI(i);
        networks += ",\"channel\":";
        networks += WiFi.channel(i);
        networks += ",\"encryption\":\"";
        switch (WiFi.encryptionType(i)) {
            case WIFI_AUTH_OPEN: networks += "Open"; break;
            case WIFI_AUTH_WEP: networks += "WEP"; break;
            case WIFI_AUTH_WPA_PSK: networks += "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: networks += "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: networks += "WPA/WPA2"; break;
            case WIFI_AUTH_WPA2_ENTERPRISE: networks += "WPA2-Enterprise"; break;
            default: networks += "Unknown"; break;
        }
        networks += "\"}";

        Serial.print("  ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.println(" dBm)");
    }
    networks += "]";

    WiFi.scanDelete();

    // Send to Salesforce - reconnect WiFi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi dropped, reconnecting...");
        WiFi.reconnect();
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(250);
            attempts++;
        }
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi reconnect failed");
            BLEDevice::startAdvertising();
            return;
        }
        Serial.println("WiFi reconnected");
    }

    // Use fresh client for each request to avoid stale SSL state
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    http.begin(secureClient, SF_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"function\":\"Scan\",";
    payload += "\"networks\":" + networks + ",";
    payload += "\"apiKey\":\"" + String(SF_API_KEY) + "\"";
    payload += "}";

    Serial.println("Sending network scan to Salesforce...");

    int httpCode = http.POST(payload);
    bool success = false;

    if (httpCode > 0) {
        String response = http.getString();
        Serial.print("Response (");
        Serial.print(httpCode);
        Serial.print("): ");
        Serial.println(response);
        if (httpCode == 200 || httpCode == 201) {
            success = true;
            beepSuccess();
        }
    } else {
        String errStr = http.errorToString(httpCode);
        Serial.print("HTTP Error: ");
        Serial.println(errStr);
        beepFail();
    }

    http.end();

    // Resume BLE advertising
    Serial.println("Resuming BLE...");
    BLEDevice::startAdvertising();
}

void updateSensorReading() {
    // Read sensors and update BLE characteristic (doesn't post to Salesforce)
    float tempC = temperatureRead();
    float temp = (tempC * 9.0 / 5.0) + 32.0;
    int moistureRaw = readSoilMoisture();
    float moisture = getMoisturePercent(moistureRaw);

    char sensorMsg[50];
    snprintf(sensorMsg, sizeof(sensorMsg), "%.1fF | %.0f%%", temp, moisture);
    if (pSensorChar) {
        pSensorChar->setValue(sensorMsg);
        if (deviceConnected) {
            pSensorChar->notify();
        }
    }
}

void loop() {
    static unsigned long lastTapTime = 0;
    static int tapCount = 0;
    static bool lastButtonState = HIGH;
    static bool buttonPressed = false;
    static bool lastButtonReading = HIGH;
    static unsigned long lastSensorUpdate = 0;

    // Handle BLE disconnection - restart advertising
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("BLE: Advertising restarted");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        // Send current WiFi status to newly connected phone
        updateWifiStatus();
    }

    // Update sensor reading every 2 seconds when phone is connected
    // Skip if paused (during HTTP requests to avoid BLE/WiFi radio conflict)
    if (deviceConnected && !pauseSensorUpdates && (millis() - lastSensorUpdate >= 2000)) {
        lastSensorUpdate = millis();
        updateSensorReading();
    }

    // Handle WiFi scan request from phone
    if (wifiScanRequested) {
        wifiScanRequested = false;
        performWifiScanForPhone();
    }

    // Handle forget network request from phone
    if (forgetNetworkRequested) {
        forgetNetworkRequested = false;
        forgetWifiNetwork(forgetSSID);
        notifyPhone(("Forgot: " + forgetSSID).c_str());
        forgetSSID = "";
    }

    // Handle new WiFi credentials from phone
    if (newCredentialsReceived) {
        newCredentialsReceived = false;
        saveWifiCredential(newSSID, newPassword);
        notifyPhone(("Saved: " + newSSID).c_str());

        // Try to connect to the new network
        Serial.println("Attempting to connect to new network...");
        notifyPhone("Connecting...");
        WiFi.disconnect();
        delay(100);

        if (tryConnect(newSSID.c_str(), newPassword.length() > 0 ? newPassword.c_str() : NULL)) {
            connectedSSID = newSSID;
            Serial.println("Connected to new network!");
            notifyPhone(("Connected: " + newSSID).c_str());
            postConnectionStatus(newSSID.c_str(), newPassword.length() == 0);
            updateWifiStatus();
        } else {
            Serial.println("Failed to connect to new network");
            notifyPhone("Connection failed");
            // Reconnect to previous network
            connectWiFi();
        }
        newSSID = "";
        newPassword = "";
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost - reconnecting...");
        notifyPhone("WiFi reconnecting...");
        connectWiFi();
    }

    bool reading = digitalRead(BUTTON_PIN);

    // Notify phone of button state changes
    if (reading != lastButtonReading) {
        notifyButtonState(reading == LOW);
        lastButtonReading = reading;
    }

    // Detect button press (HIGH to LOW transition)
    if (reading == LOW && lastButtonState == HIGH) {
        buttonPressed = true;
    }
    lastButtonState = reading;

    // Process button press with simple debounce
    if (buttonPressed) {
        buttonPressed = false;
        delay(50);  // Simple debounce

        unsigned long now = millis();

        if (now - lastTapTime > TAP_WINDOW) {
            // New tap sequence
            tapCount = 1;
            Serial.println("Tap 1");
            notifyPhone("Tap 1...");
            beep(20);
        } else {
            tapCount++;
            Serial.print("Tap ");
            Serial.println(tapCount);
            char tapMsg[20];
            snprintf(tapMsg, sizeof(tapMsg), "Tap %d...", tapCount);
            notifyPhone(tapMsg);
            beep(20);
        }
        lastTapTime = now;

        // Triple tap triggers immediately
        if (tapCount >= 3) {
            Serial.println("\n*** TRIPLE TAP! ***");
            notifyPhone("Triple tap!");
            scanAndSendNetworks();
            tapCount = 0;
            lastTapTime = 0;
        }
    }

    // Check for timeout to trigger single or double tap
    if (tapCount > 0 && tapCount < 3 && (millis() - lastTapTime) >= TAP_WINDOW) {
        if (tapCount == 1) {
            Serial.println("\n*** SINGLE TAP! ***");
            notifyPhone("Single tap!");
            sendReading("Single");
        } else if (tapCount == 2) {
            Serial.println("\n*** DOUBLE TAP! ***");
            notifyPhone("Double tap!");
            sendReading("Double");
        }
        tapCount = 0;
    }

    // Touch sensor detection
    static bool wasTouched = false;
    static unsigned long lastTouchTime = 0;
    int touchValue = touchRead(TOUCH_PIN);

    if (touchValue < TOUCH_THRESHOLD && !wasTouched) {
        // Debounce - must be 200ms since last touch
        if (millis() - lastTouchTime > 200) {
            wasTouched = true;
            lastTouchTime = millis();
            Serial.print("\n*** TOUCH DETECTED! (value: ");
            Serial.print(touchValue);
            Serial.println(") ***");
            notifyPhone("Touch!");
            notifyButtonState(true);
            sendReading("Touch");
            notifyButtonState(false);
        }
    } else if (touchValue >= TOUCH_THRESHOLD) {
        wasTouched = false;
    }

    delay(10);
}
