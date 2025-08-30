/**
 * @file pulse_monitor.ino
 * @description This file is executed server-side (ESP32) It is responsible for generating a Wi-Fi access point,
 * serving the visualisation webpage and acquiring measurements.
 *
 * @author Matthieu Bouveron
 * @copyright 2025 Matthieu Bouveron
 * @license MIT
 *
 * This file is part of ESP32_Pulse_Monitor.
 *
 * ESP32_Pulse_Monitor is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/queue.h>

#define INPUT_PIN 13 // Signal wire on D19

// Define the access point credentials
const char* ssid = "ESP32-AP";
const char* password = "password";

// Create a web server on port 80
WebServer server(80);

// Define a queue handle
QueueHandle_t timestampQueue;

void IRAM_ATTR rx_interrupt_routine() {
    static uint32_t risingEdgeTime = 0;
    if (digitalRead(INPUT_PIN) == HIGH) {
        risingEdgeTime = micros();
    } else {
        uint32_t fallingEdgeTime = micros();
        // Send both timestamps to the queue
        uint32_t timestamps[2] = {risingEdgeTime, fallingEdgeTime};
        xQueueSendFromISR(timestampQueue, timestamps, NULL);
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    // Set up the access point
    WiFi.softAP(ssid, password);
    Serial.println("Access Point started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Create a queue to hold timestamp pairs
    timestampQueue = xQueueCreate(10, sizeof(uint32_t[2]));

    // Execute function when INPUT_PIN state changes
    attachInterrupt(INPUT_PIN, rx_interrupt_routine, CHANGE);

    // Set up the web server
    server.on("/", handleRoot);
    server.on("/data", handleData);

    // Serve static files
    server.serveStatic("/", LittleFS, "/");
    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
}

void handleRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        server.send(500, "text/plain", "Failed to open file");
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

void handleData() {
    JsonDocument response;
    JsonArray responseArray = response.to<JsonArray>();

    uint32_t timestamps[2];
    while (xQueueReceive(timestampQueue, &timestamps, 0) == pdTRUE) {
        JsonArray pair = responseArray.add<JsonArray>();
        pair.add(timestamps[0]);
        pair.add(timestamps[1]);
    }

    String response_string;
    serializeJson(response, response_string);
    server.send(200, "text/plain", response_string);

    Serial.println(response_string);

    // Clear the response after sending
    response.clear();
}
