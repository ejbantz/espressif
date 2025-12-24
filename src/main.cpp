#include <Arduino.h>
#include <WiFi.h>
#include "credentials.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 WiFi Connection Test");
    Serial.println("================================");

    // Set WiFi mode to station (client)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Scan for networks first
    Serial.println("Scanning for networks...");
    int numNetworks = WiFi.scanNetworks();
    Serial.print("Found ");
    Serial.print(numNetworks);
    Serial.println(" networks:");

    for (int i = 0; i < numNetworks; i++) {
        Serial.print("  ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.println(" dBm)");
    }
    Serial.println();

    Serial.print("Connecting to: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Wait for connection with timeout
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
        Serial.print("Signal Strength (RSSI): ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        Serial.print("MAC Address: ");
        Serial.println(WiFi.macAddress());
    } else {
        Serial.println("WiFi Connection FAILED");
        Serial.print("Status code: ");
        Serial.println(WiFi.status());
    }
}

void loop() {
    // Check connection status every 5 seconds
    static unsigned long lastCheck = 0;

    if (millis() - lastCheck > 5000) {
        lastCheck = millis();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("WiFi OK - IP: ");
            Serial.print(WiFi.localIP());
            Serial.print(" | RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        } else {
            Serial.println("WiFi Disconnected - Reconnecting...");
            WiFi.reconnect();
        }
    }
}
