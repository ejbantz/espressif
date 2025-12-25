#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "credentials.h"

// BLE Configuration
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BUTTON_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATUS_CHAR_UUID    "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"

BLEServer* pServer = NULL;
BLECharacteristic* pButtonChar = NULL;
BLECharacteristic* pStatusChar = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE: Phone connected");
    };
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE: Phone disconnected");
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

// Salesforce API Configuration
const char* SF_ENDPOINT = "https://ejdev-dev-ed.develop.my.site.com/vforcesite/services/apexrest/sensor/reading";
const char* SF_API_KEY = "LawnMonitor2024SecretKey";
const char* DEVICE_ID = "ESP32-001";

// Boot button on GPIO0
const int BUTTON_PIN = 0;

// Multi-tap timing (ms)
const unsigned long TAP_WINDOW = 400;
const unsigned long DEBOUNCE_TIME = 50;

WiFiClientSecure client;

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.print("Connecting to: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi Connection FAILED");
    }
}

bool sendToSalesforce(float temperature, float humidity, const char* function) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return false;
    }

    HTTPClient http;
    client.setInsecure();

    http.begin(client, SF_ENDPOINT);
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

    int httpCode = http.POST(payload);

    if (httpCode > 0) {
        String response = http.getString();
        Serial.print("Response (");
        Serial.print(httpCode);
        Serial.print("): ");
        Serial.println(response);
        http.end();
        return httpCode == 200 || httpCode == 201;
    } else {
        Serial.print("HTTP Error: ");
        Serial.println(http.errorToString(httpCode));
        http.end();
        return false;
    }
}

void setupBLE() {
    Serial.println("Starting BLE...");

    BLEDevice::init("ESP32-Sensor");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

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
    Serial.println("Single tap = Send reading");
    Serial.println("Double tap = Send reading");
    Serial.println("Triple tap = Scan WiFi networks");
    Serial.println("================================");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    setupBLE();
    connectWiFi();
}

void sendReading(const char* function) {
    float temp = 72.0 + (random(0, 100) / 10.0);
    float humidity = 40.0 + (random(0, 200) / 10.0);

    Serial.println("\n--- Sending Sensor Data ---");
    Serial.print("Function: ");
    Serial.println(function);
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" F");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    // Notify phone what we're sending
    char statusMsg[100];
    snprintf(statusMsg, sizeof(statusMsg), "Sending %s: %.1fF, %.1f%%", function, temp, humidity);
    notifyPhone(statusMsg);

    if (sendToSalesforce(temp, humidity, function)) {
        Serial.println("Success!");
        notifyPhone("Salesforce: Success!");
    } else {
        Serial.println("Failed to send");
        notifyPhone("Salesforce: Failed!");
    }
}

void scanAndSendNetworks() {
    Serial.println("\n--- Scanning WiFi Networks ---");
    notifyPhone("Scanning WiFi...");

    int numNetworks = WiFi.scanNetworks();
    Serial.print("Found ");
    Serial.print(numNetworks);
    Serial.println(" networks");

    char scanMsg[50];
    snprintf(scanMsg, sizeof(scanMsg), "Found %d networks", numNetworks);
    notifyPhone(scanMsg);

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

    // Send to Salesforce
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return;
    }

    HTTPClient http;
    client.setInsecure();
    http.begin(client, SF_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"function\":\"Scan\",";
    payload += "\"networks\":" + networks + ",";
    payload += "\"apiKey\":\"" + String(SF_API_KEY) + "\"";
    payload += "}";

    Serial.println("Sending network scan to Salesforce...");

    int httpCode = http.POST(payload);

    if (httpCode > 0) {
        String response = http.getString();
        Serial.print("Response (");
        Serial.print(httpCode);
        Serial.print("): ");
        Serial.println(response);
    } else {
        Serial.print("HTTP Error: ");
        Serial.println(http.errorToString(httpCode));
    }
    http.end();
}

void loop() {
    static unsigned long lastTapTime = 0;
    static int tapCount = 0;
    static bool lastButtonState = HIGH;
    static bool buttonPressed = false;
    static bool lastButtonReading = HIGH;

    // Handle BLE disconnection - restart advertising
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("BLE: Advertising restarted");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
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
        } else {
            tapCount++;
            Serial.print("Tap ");
            Serial.println(tapCount);
            char tapMsg[20];
            snprintf(tapMsg, sizeof(tapMsg), "Tap %d...", tapCount);
            notifyPhone(tapMsg);
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

    delay(10);
}
