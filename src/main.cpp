#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "credentials.h"

// Salesforce API Configuration
const char* SF_ENDPOINT = "https://ejdev-dev-ed.develop.my.site.com/vforcesite/services/apexrest/sensor/reading";
const char* SF_API_KEY = "LawnMonitor2024SecretKey";
const char* DEVICE_ID = "ESP32-001";

// Boot button on GPIO0
const int BUTTON_PIN = 0;

// Double-tap timing (ms)
const unsigned long DOUBLE_TAP_WINDOW = 400;
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

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 Salesforce IoT Device");
    Serial.println("================================");
    Serial.println("Single tap = Single function");
    Serial.println("Double tap = Double function");
    Serial.println("================================");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

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

    if (sendToSalesforce(temp, humidity, function)) {
        Serial.println("Success!");
    } else {
        Serial.println("Failed to send");
    }
}

void loop() {
    static unsigned long firstTapTime = 0;
    static bool waitingForSecondTap = false;
    static bool lastButtonState = HIGH;
    static unsigned long lastDebounceTime = 0;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost - reconnecting...");
        connectWiFi();
    }

    bool reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_TIME) {
        static bool buttonState = HIGH;

        if (reading != buttonState) {
            buttonState = reading;

            if (buttonState == LOW) {
                if (waitingForSecondTap && (millis() - firstTapTime) < DOUBLE_TAP_WINDOW) {
                    Serial.println("\n*** DOUBLE TAP! ***");
                    waitingForSecondTap = false;
                    sendReading("Double");
                } else {
                    firstTapTime = millis();
                    waitingForSecondTap = true;
                }
            }
        }
    }

    lastButtonState = reading;

    if (waitingForSecondTap && (millis() - firstTapTime) >= DOUBLE_TAP_WINDOW) {
        Serial.println("\n*** SINGLE TAP! ***");
        waitingForSecondTap = false;
        sendReading("Single");
    }

    delay(5);
}
