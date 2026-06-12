#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "freertos/semphr.h"
#include "WS_Information.h"
#include "WS_Relay.h"
#include "WS_WIFI.h"

// ─── MQTTManager ─────────────────────────────────────────────────────────────
// Single-instance MQTT client with:
//  - Non-blocking exponential-backoff reconnection
//  - FreeRTOS mutex for thread-safe publish
//  - ArduinoJson-based incoming message parsing
//  - Configurable keep-alive and socket timeout
class MQTTManager {
public:
    MQTTManager();

    // Starts WiFi then spawns the MQTT task.  Call once from setup().
    void init();

    // Thread-safe publish.  Adds the device ID then serialises to JSON.
    // Returns false if not connected or the broker rejects the message.
    bool publish(JsonDocument& doc);

    bool isConnected();

private:
    static constexpr uint32_t RETRY_MIN_MS   = 1000;
    static constexpr uint32_t RETRY_MAX_MS   = 30000;
    static constexpr size_t   PUB_BUF_SIZE   = 512;
    static constexpr int      KEEPALIVE_SECS = 60;
    static constexpr int      SOCKET_TIMEOUT = 5;   // seconds

    WiFiClient         _wifi;
    PubSubClient       _client;
    SemaphoreHandle_t  _mutex;
    uint32_t           _retryDelayMs;
    uint32_t           _nextRetryAt;
    bool               _wasConnected;

    void taskLoop();
    void tryConnect();
    void onMessage(const char* topic, const uint8_t* payload, uint32_t len);

    // Static shims required by C-style PubSubClient callback API
    static void mqttCallback(char* topic, byte* payload, unsigned int len);
    static void mqttTask(void* param);
    static MQTTManager* _instance;
};

// ─── Free-function API (called from main.cpp) ─────────────────────────────────
void MQTT_Init();
void sendJsonData(JsonDocument& doc);

