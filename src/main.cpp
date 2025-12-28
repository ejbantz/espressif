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
#include "esp_wifi.h"
#include "credentials.h"

// TinyGSM for SIM7000A cellular modem
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

// SIM7000A pins
#define MODEM_TX 17  // ESP32 TX → Modem RX
#define MODEM_RX 16  // Modem TX → ESP32 RX
// #define MODEM_PWRKEY 26  // Optional - comment out if not wired, press PWR button manually
#define SerialAT Serial2

// Hologram APN
const char apn[] = "hologram";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Modem instance
TinyGsm modem(SerialAT);
TinyGsmClient cellularClient(modem);
bool modemInitialized = false;

// GPS data
float gpsLatitude = 0.0;
float gpsLongitude = 0.0;
bool gpsValid = false;

// Forward declarations for Salesforce config
extern const char* SF_ENDPOINT;
extern const char* SF_API_KEY;
extern const char* DEVICE_ID;

// BLE Configuration
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BUTTON_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATUS_CHAR_UUID    "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define WIFI_SCAN_CHAR_UUID "a1e8f5d2-7b3c-4e9a-8f2d-6c5b4a3d2e1f"
#define WIFI_CRED_CHAR_UUID "b2f9e6c3-8c4d-5f0b-9e3e-7d6c5b4a3f20"
#define WIFI_STATUS_CHAR_UUID "c3a0f7d4-9d5e-6f1c-0a4f-8e7d6c5b4a31"
#define SENSOR_CHAR_UUID "d4b1e8f5-0e6f-7a2d-1b5a-9f8e7d6c5b42"
#define GPS_CHAR_UUID "f6d3a9b7-2c4e-5f6a-8b9c-0d1e2f3a4b53"
#define CELL_CHAR_UUID "a7e4b0c8-3d5f-6a7b-9c0d-1e2f3a4b5c64"

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
BLECharacteristic* pGpsChar = NULL;
BLECharacteristic* pCellChar = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool wifiScanRequested = false;
bool newCredentialsReceived = false;
bool forgetNetworkRequested = false;
bool pauseSensorUpdates = false;  // Pause during HTTP requests
bool bleEnabled = false;  // BLE off by default for reliable HTTP
String newSSID = "";
String newPassword = "";
String forgetSSID = "";

void beepBleConnect();  // Forward declaration
void beepBleDisconnect();
void beepWifiConnect();
void beepFail();
void setupBLE();

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
    if (pStatusChar) {
        // Always update value so phone sees it when reconnecting
        pStatusChar->setValue(status);
        if (deviceConnected) {
            pStatusChar->notify();
        }
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

void beepBleOn() {
    // Rising three-tone - BLE enabled
    playTone(440, 80);
    delay(50);
    playTone(660, 80);
    delay(50);
    playTone(880, 120);
}

void beepBleOff() {
    // Descending three-tone - BLE disabled
    playTone(880, 80);
    delay(50);
    playTone(660, 80);
    delay(50);
    playTone(440, 120);
}

void beepCellular() {
    // Quick double chirp - cellular activity
    playTone(600, 40);
    delay(30);
    playTone(800, 60);
}

// Modem power control (optional - press PWR button on SIM7000A board if not wired)
void modemPowerOn() {
    // Skip if PWRKEY not connected - user presses button manually
    #ifdef MODEM_PWRKEY
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(2000);
    digitalWrite(MODEM_PWRKEY, LOW);
    Serial.println("Modem power key toggled");
    #else
    Serial.println("PWRKEY not wired - press PWR button on SIM7000A board");
    #endif
}

// Initialize cellular modem
bool initModem() {
    if (modemInitialized) return true;

    Serial.println("Initializing SIM7000A modem...");
    SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);

    modemPowerOn();
    delay(5000);

    Serial.println("Testing modem communication...");
    if (!modem.testAT()) {
        Serial.println("Modem not responding, trying again...");
        modemPowerOn();
        delay(5000);
        if (!modem.testAT()) {
            Serial.println("Modem failed to respond");
            return false;
        }
    }

    Serial.println("Modem OK");
    String modemInfo = modem.getModemInfo();
    Serial.print("Modem Info: ");
    Serial.println(modemInfo);

    // Enable GPS
    Serial.println("Enabling GPS...");
    modem.sendAT("+CGNSPWR=1");  // Power on GPS
    modem.waitResponse();
    delay(1000);

    modemInitialized = true;
    return true;
}

// Connect to cellular network
bool connectCellular() {
    if (!modemInitialized && !initModem()) {
        return false;
    }

    Serial.println("Connecting to cellular network...");

    // Check SIM card
    if (modem.getSimStatus() != 1) {
        Serial.println("SIM card not detected");
        return false;
    }
    Serial.println("SIM OK");

    // Wait for network registration
    Serial.print("Waiting for network...");
    int timeout = 60;
    while (!modem.isNetworkConnected() && timeout > 0) {
        Serial.print(".");
        delay(1000);
        timeout--;
    }
    Serial.println();

    if (!modem.isNetworkConnected()) {
        Serial.println("Network registration failed");
        return false;
    }
    Serial.println("Network registered");

    // Connect GPRS
    Serial.print("Connecting GPRS...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("GPRS connection failed");
        return false;
    }
    Serial.println("GPRS connected!");

    beepCellular();
    return true;
}

// Read GPS coordinates
bool updateGPS() {
    if (!modemInitialized) return false;

    // Request GPS info
    modem.sendAT("+CGNSINF");
    if (modem.waitResponse(10000L, "+CGNSINF:") != 1) {
        return false;
    }

    // Parse CGNSINF response: run,fix,date,time,lat,lon,alt,speed,course,...
    String gpsData = SerialAT.readStringUntil('\n');
    Serial.print("GPS raw: ");
    Serial.println(gpsData);

    // Parse fields
    int commaIndex = 0;
    String fields[10];
    int fieldCount = 0;
    int lastIndex = 0;

    for (int i = 0; i < gpsData.length() && fieldCount < 10; i++) {
        if (gpsData.charAt(i) == ',') {
            fields[fieldCount++] = gpsData.substring(lastIndex, i);
            lastIndex = i + 1;
        }
    }
    if (lastIndex < gpsData.length() && fieldCount < 10) {
        fields[fieldCount++] = gpsData.substring(lastIndex);
    }

    // Field 1 = fix status (1 = valid fix)
    if (fieldCount >= 6 && fields[1] == "1") {
        gpsLatitude = fields[3].toFloat();
        gpsLongitude = fields[4].toFloat();
        gpsValid = true;
        Serial.print("GPS: ");
        Serial.print(gpsLatitude, 6);
        Serial.print(", ");
        Serial.println(gpsLongitude, 6);
        return true;
    }

    gpsValid = false;
    Serial.println("No GPS fix yet");
    return false;
}

// Send data via cellular HTTP
bool sendViaCellular(float temperature, float humidity, const char* function) {
    if (!modemInitialized) {
        Serial.println("Modem not initialized");
        if (!initModem()) return false;
    }

    if (!modem.isGprsConnected()) {
        Serial.println("GPRS not connected, reconnecting...");
        if (!connectCellular()) return false;
    }

    // Try to get GPS fix
    updateGPS();

    Serial.println("Sending via cellular HTTP...");
    beepCellular();

    // Build JSON payload
    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"function\":\"" + String(function) + "\",";
    payload += "\"connectionType\":\"Cellular\",";
    if (gpsValid) {
        payload += "\"latitude\":" + String(gpsLatitude, 6) + ",";
        payload += "\"longitude\":" + String(gpsLongitude, 6) + ",";
    }
    payload += "\"apiKey\":\"" + String(SF_API_KEY) + "\"";
    payload += "}";

    Serial.println(payload);

    // Use TinyGSM HTTP
    modem.sendAT("+HTTPINIT");
    if (modem.waitResponse() != 1) {
        Serial.println("HTTP init failed");
        return false;
    }

    modem.sendAT("+HTTPPARA=\"CID\",1");
    modem.waitResponse();

    String urlCmd = "+HTTPPARA=\"URL\",\"" + String(SF_ENDPOINT) + "\"";
    modem.sendAT(urlCmd.c_str());
    modem.waitResponse();

    modem.sendAT("+HTTPPARA=\"CONTENT\",\"application/json\"");
    modem.waitResponse();

    // Set POST data
    String dataCmd = "+HTTPDATA=" + String(payload.length()) + ",10000";
    modem.sendAT(dataCmd.c_str());
    if (modem.waitResponse(10000L, "DOWNLOAD") != 1) {
        Serial.println("HTTP data setup failed");
        modem.sendAT("+HTTPTERM");
        modem.waitResponse();
        return false;
    }

    SerialAT.print(payload);
    delay(1000);

    // Execute POST
    modem.sendAT("+HTTPACTION=1");  // 1 = POST
    if (modem.waitResponse(30000L, "+HTTPACTION:") != 1) {
        Serial.println("HTTP POST timeout");
        modem.sendAT("+HTTPTERM");
        modem.waitResponse();
        return false;
    }

    String response = SerialAT.readStringUntil('\n');
    Serial.print("HTTP response: ");
    Serial.println(response);

    // Parse response: +HTTPACTION: method,status,length
    int status = 0;
    int firstComma = response.indexOf(',');
    int secondComma = response.indexOf(',', firstComma + 1);
    if (firstComma > 0 && secondComma > firstComma) {
        status = response.substring(firstComma + 1, secondComma).toInt();
    }

    modem.sendAT("+HTTPTERM");
    modem.waitResponse();

    if (status == 200 || status == 201) {
        Serial.println("Cellular POST success!");
        return true;
    }

    Serial.print("Cellular POST failed with status: ");
    Serial.println(status);
    return false;
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
const unsigned long TAP_WINDOW = 600;  // Time between taps (ms)
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

// BLE characteristic for sending data to phone for Salesforce posting
BLECharacteristic* pSalesforceChar = NULL;
#define SALESFORCE_CHAR_UUID "e5c2f8a6-1b3d-4e5f-9a7c-8d6b5e4f3a21"

bool sendDirectToSalesforce(float temperature, float humidity, const char* function) {
    // Direct WiFi HTTP - only called when BLE is disabled
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return false;
    }

    Serial.println("Sending direct to Salesforce via WiFi...");

    // Try to get GPS if modem is available
    if (modemInitialized) {
        updateGPS();
    }

    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(client, SF_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"function\":\"" + String(function) + "\",";
    payload += "\"connectionType\":\"WiFi\",";
    if (gpsValid) {
        payload += "\"latitude\":" + String(gpsLatitude, 6) + ",";
        payload += "\"longitude\":" + String(gpsLongitude, 6) + ",";
    }
    payload += "\"apiKey\":\"" + String(SF_API_KEY) + "\"";
    payload += "}";

    Serial.println(payload);
    int httpCode = http.POST(payload);
    bool success = (httpCode == 200 || httpCode == 201);

    if (success) {
        Serial.println("WiFi POST success!");
    } else {
        Serial.print("WiFi POST failed: ");
        Serial.println(httpCode);
    }

    http.end();
    return success;
}

void sendSensorData(float temperature, float humidity, const char* function) {
    // Priority 1: Phone (BLE relay)
    if (bleEnabled && deviceConnected && pSalesforceChar) {
        // Get GPS for phone to include in POST
        if (modemInitialized) {
            updateGPS();
        }

        String payload = "{";
        payload += "\"temperature\":" + String(temperature, 1) + ",";
        payload += "\"humidity\":" + String(humidity, 1) + ",";
        payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
        payload += "\"function\":\"" + String(function) + "\"";
        if (gpsValid) {
            payload += ",\"latitude\":" + String(gpsLatitude, 6);
            payload += ",\"longitude\":" + String(gpsLongitude, 6);
        }
        payload += "}";

        Serial.println("Priority 1: Sending via Phone (BLE)");
        Serial.println(payload);

        pSalesforceChar->setValue(payload.c_str());
        pSalesforceChar->notify();

        notifyPhone("Sent via Phone");
        beepSuccess();
        return;
    }

    // Priority 2: WiFi (direct HTTP)
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Priority 2: Trying WiFi...");
        if (sendDirectToSalesforce(temperature, humidity, function)) {
            beepSuccess();
            return;
        }
        Serial.println("WiFi failed, trying cellular...");
    } else {
        Serial.println("WiFi not connected, trying cellular...");
    }

    // Priority 3: Cellular (SIM7000A)
    Serial.println("Priority 3: Trying Cellular...");
    if (sendViaCellular(temperature, humidity, function)) {
        beepSuccess();
    } else {
        Serial.println("All connection methods failed!");
        beepFail();
    }
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

    // GPS status characteristic
    pGpsChar = pService->createCharacteristic(
        GPS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pGpsChar->addDescriptor(new BLE2902());
    pGpsChar->setValue("No modem");

    // Cellular status characteristic
    pCellChar = pService->createCharacteristic(
        CELL_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pCellChar->addDescriptor(new BLE2902());
    pCellChar->setValue("No modem");

    // Salesforce POST characteristic - sends data to phone for posting
    pSalesforceChar = pService->createCharacteristic(
        SALESFORCE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pSalesforceChar->addDescriptor(new BLE2902());
    pSalesforceChar->setValue("{}");

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

    // Connect WiFi first
    connectWiFi();

    // Initialize cellular modem (for GPS and cellular fallback)
    Serial.println("\nInitializing cellular modem...");
    if (initModem()) {
        Serial.println("Modem ready - GPS enabled");
        // Try to get initial GPS fix
        updateGPS();
    } else {
        Serial.println("Modem init failed - check wiring and press PWR button");
    }

    // BLE stays OFF by default for reliable direct HTTP
    // User can 4-tap to enable BLE for phone configuration
    Serial.println("BLE disabled - 4-tap to enable");
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
    snprintf(statusMsg, sizeof(statusMsg), "Reading: %.1fF, %.0f%%", temp, humidity);
    notifyPhone(statusMsg);

    // Send via best available method: Phone → WiFi → Cellular
    sendSensorData(temp, humidity, function);
}

void scanAndSendNetworks() {
    Serial.println("\n--- Triple Tap: WiFi Scan ---");

    // Just send a scan reading to Salesforce via phone
    float tempC = temperatureRead();
    float temp = (tempC * 9.0 / 5.0) + 32.0;
    int moistureRaw = analogRead(34);
    float moisture = 0;  // Simple read for scan

    notifyPhone("Triple tap - scanning");
    sendSensorData(temp, moisture, "Scan");
}

void updateGpsStatus() {
    if (!pGpsChar) return;

    char gpsMsg[50];
    if (!modemInitialized) {
        snprintf(gpsMsg, sizeof(gpsMsg), "No modem");
    } else if (gpsValid) {
        snprintf(gpsMsg, sizeof(gpsMsg), "%.4f, %.4f", gpsLatitude, gpsLongitude);
    } else {
        snprintf(gpsMsg, sizeof(gpsMsg), "Searching...");
    }

    pGpsChar->setValue(gpsMsg);
    if (deviceConnected) {
        pGpsChar->notify();
    }
}

void updateCellStatus() {
    if (!pCellChar) return;

    char cellMsg[50];
    if (!modemInitialized) {
        snprintf(cellMsg, sizeof(cellMsg), "No modem");
    } else if (modem.isGprsConnected()) {
        int csq = modem.getSignalQuality();
        snprintf(cellMsg, sizeof(cellMsg), "Connected (CSQ:%d)", csq);
    } else if (modem.isNetworkConnected()) {
        snprintf(cellMsg, sizeof(cellMsg), "Registered");
    } else {
        snprintf(cellMsg, sizeof(cellMsg), "Searching...");
    }

    pCellChar->setValue(cellMsg);
    if (deviceConnected) {
        pCellChar->notify();
    }
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

    // Also update GPS and cellular status
    updateGpsStatus();
    updateCellStatus();
}

void loop() {
    static unsigned long lastTapTime = 0;
    static int tapCount = 0;
    static bool lastButtonState = HIGH;
    static bool buttonPressed = false;
    static bool lastButtonReading = HIGH;
    static unsigned long lastSensorUpdate = 0;

    // Handle BLE disconnection - fully disable BLE to allow direct HTTP
    if (bleEnabled && !deviceConnected && oldDeviceConnected) {
        delay(500);
        Serial.println("Phone disconnected - fully disabling BLE");
        BLEDevice::deinit(false);  // Deinit but keep memory
        btStop();  // Stop Bluetooth controller completely
        bleEnabled = false;
        beepBleOff();
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        // Send current WiFi status to newly connected phone
        updateWifiStatus();
    }

    // Update sensor reading every 2 seconds when phone is connected
    if (bleEnabled && deviceConnected && !pauseSensorUpdates && (millis() - lastSensorUpdate >= 2000)) {
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

        // 4-tap triggers BLE toggle immediately
        if (tapCount >= 4) {
            if (!bleEnabled) {
                Serial.println("\n*** 4-TAP: ENABLING BLE ***");
                btStart();  // Start Bluetooth controller
                setupBLE();
                bleEnabled = true;
                beepBleOn();
            } else {
                Serial.println("\n*** 4-TAP: DISABLING BLE ***");
                BLEDevice::deinit(false);
                btStop();
                bleEnabled = false;
                beepBleOff();
            }
            tapCount = 0;
            lastTapTime = 0;
        }
    }

    // Check for timeout to trigger single, double, or triple tap
    if (tapCount > 0 && tapCount < 4 && (millis() - lastTapTime) >= TAP_WINDOW) {
        if (tapCount == 1) {
            Serial.println("\n*** SINGLE TAP! ***");
            sendReading("Single");
        } else if (tapCount == 2) {
            Serial.println("\n*** DOUBLE TAP! ***");
            sendReading("Double");
        } else if (tapCount == 3) {
            Serial.println("\n*** TRIPLE TAP! ***");
            sendReading("Scan");
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
