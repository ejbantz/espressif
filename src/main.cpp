#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "credentials.h"

// Salesforce API Configuration
const char* SF_ENDPOINT = "https://ejdev-dev-ed.develop.my.site.com/vforcesite/services/apexrest/sensor/reading";
const char* SF_API_KEY = "LawnMonitor2024SecretKey";
const char* DEVICE_ID = "ESP32-001";

// Send interval (30 seconds)
const unsigned long SEND_INTERVAL = 30000;

// Boot button on GPIO0
const int BUTTON_PIN = 0;

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

bool sendToSalesforce(float temperature, float humidity) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return false;
    }

    HTTPClient http;

    // Skip SSL certificate verification (for development)
    client.setInsecure();

    http.begin(client, SF_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    // Build JSON payload
    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
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

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 Salesforce IoT Device");
    Serial.println("================================");

    // Setup button input (Boot button has external pull-up)
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    connectWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        // Send initial reading on boot
        Serial.println("\nSending initial reading...");
        float temp = 72.0 + (random(0, 100) / 10.0);  // Simulated temp
        float humidity = 40.0 + (random(0, 200) / 10.0);  // Simulated humidity
        sendToSalesforce(temp, humidity);
    }
}

void sendReading() {
    // Simulated sensor values (replace with real sensor readings)
    float temp = 72.0 + (random(0, 100) / 10.0);
    float humidity = 40.0 + (random(0, 200) / 10.0);

    Serial.println("\n--- Sending Sensor Data ---");
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" F");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    if (sendToSalesforce(temp, humidity)) {
        Serial.println("Success!");
    } else {
        Serial.println("Failed to send");
    }
}

void loop() {
    static unsigned long lastSend = 0;
    static unsigned long lastButtonPress = 0;
    static bool lastButtonState = HIGH;

    // Reconnect WiFi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost - reconnecting...");
        connectWiFi();
    }

    // Check button press (active LOW, with debounce)
    bool buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH && (millis() - lastButtonPress > 500)) {
        lastButtonPress = millis();
        Serial.println("\n*** Button Pressed! ***");
        sendReading();
    }
    lastButtonState = buttonState;

    // Send data every SEND_INTERVAL
    if (millis() - lastSend > SEND_INTERVAL) {
        lastSend = millis();
        sendReading();
    }

    delay(10);
}
