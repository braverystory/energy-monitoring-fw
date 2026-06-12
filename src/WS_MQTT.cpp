#include "WS_MQTT.h"
#include <string.h>

// ─── Singleton pointer (needed for static PubSubClient callback shim) ─────────
MQTTManager* MQTTManager::_instance = nullptr;

// ─── Construction ────────────────────────────────────────────────────────────

MQTTManager::MQTTManager()
    : _client(_wifi),
      _mutex(nullptr),
      _retryDelayMs(RETRY_MIN_MS),
      _nextRetryAt(0),
      _wasConnected(false)
{
    _instance = this;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void MQTTManager::init() {
    _mutex = xSemaphoreCreateMutex();
    configASSERT(_mutex);

    _client.setServer(MQTT_Server, MQTT_Port);
    _client.setCallback(mqttCallback);
    _client.setKeepAlive(KEEPALIVE_SECS);
    _client.setSocketTimeout(SOCKET_TIMEOUT);

    WIFI_Init();

    xTaskCreatePinnedToCore(mqttTask, "MQTTTask", 4096, this, 3, nullptr, 0);
}

bool MQTTManager::publish(JsonDocument& doc) {
    if (!isConnected()) {
        printf("MQTT: publish skipped – not connected\r\n");
        return false;
    }

    doc["ID"] = MQTT_ID;

    char buf[PUB_BUF_SIZE];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) {
        printf("MQTT: serialization produced empty output\r\n");
        return false;
    }
    if (len >= sizeof(buf)) {
        printf("MQTT: payload exceeds buffer (%u bytes), not published\r\n", (unsigned)len);
        return false;
    }

    bool ok = false;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        ok = _client.publish(MQTT_Pub, buf);
        xSemaphoreGive(_mutex);
    } else {
        printf("MQTT: could not acquire mutex for publish\r\n");
    }

    if (!ok) printf("MQTT: broker rejected publish (check topic/QoS/payload size)\r\n");
    return ok;
}

bool MQTTManager::isConnected() {
    return _client.connected();
}

// ─── Task ─────────────────────────────────────────────────────────────────────

void MQTTManager::mqttTask(void* param) {
    static_cast<MQTTManager*>(param)->taskLoop();
    vTaskDelete(nullptr);
}

void MQTTManager::taskLoop() {
    while (true) {
        if (WIFI_Connection) {
            if (_client.connected()) {
                if (!_wasConnected) {
                    _wasConnected = true;
                    _retryDelayMs = RETRY_MIN_MS;   // reset backoff
                }
                if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    _client.loop();
                    xSemaphoreGive(_mutex);
                }
            } else {
                if (_wasConnected) {
                    printf("MQTT: disconnected (state=%d)\r\n", _client.state());
                    _wasConnected = false;
                }
                uint32_t now = (uint32_t)millis();
                if (now >= _nextRetryAt) {
                    tryConnect();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void MQTTManager::tryConnect() {
    printf("MQTT: connecting to %s:%d ...\r\n", MQTT_Server, MQTT_Port);

    if (_client.connect(MQTT_ID)) {
        if (_client.subscribe(MQTT_Sub)) {
            printf("MQTT: connected, subscribed to [%s]\r\n", MQTT_Sub);
        } else {
            printf("MQTT: connected but subscribe failed for [%s]\r\n", MQTT_Sub);
        }
        _retryDelayMs = RETRY_MIN_MS;
    } else {
        printf("MQTT: connect failed (state=%d), retry in %lu ms\r\n",
               _client.state(), (unsigned long)_retryDelayMs);
        _nextRetryAt  = (uint32_t)millis() + _retryDelayMs;
        _retryDelayMs = (_retryDelayMs * 2 < RETRY_MAX_MS) ? _retryDelayMs * 2 : RETRY_MAX_MS;
    }
}

// ─── Incoming message handler ─────────────────────────────────────────────────

void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int len) {
    if (_instance) _instance->onMessage(topic, payload, (uint32_t)len);
}

void MQTTManager::onMessage(const char* /*topic*/, const uint8_t* payload, uint32_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, len);
    if (err) {
        printf("MQTT: JSON parse error – %s\r\n", err.c_str());
        return;
    }

    JsonObject data = doc["data"];
    if (data.isNull()) {
        printf("MQTT: missing 'data' field\r\n");
        return;
    }

    for (JsonPair kv : data) {
        const char* key   = kv.key().c_str();
        int         value = kv.value().as<int>();

        // Determine requested channel
        uint8_t ch = 0;
        if      (strcmp(key, "CH1") == 0) ch = 1;
        else if (strcmp(key, "ALL") == 0) ch = 9;
        else {
            printf("MQTT: unknown key '%s' in data object\r\n", key);
            continue;
        }

        // Compute current aggregate relay state (bounds-safe)
        bool allOn  = true;
        bool allOff = true;
        for (uint8_t i = 0; i < Relay_Number_MAX; i++) {
            if (!Relay_Flag[i]) allOn  = false;
            if ( Relay_Flag[i]) allOff = false;
        }

        if (ch >= 1 && ch <= Relay_Number_MAX) {
            // Single-channel toggle – only act if state differs
            if ((value == 1 && !Relay_Flag[ch - 1]) ||
                (value == 0 &&  Relay_Flag[ch - 1])) {
                uint8_t buf[1] = { (uint8_t)(ch + 48) };
                Relay_Analysis(buf, MQTT_Mode);
            }
        } else if (ch == 9) {
            if      (value == 1 && !allOn)  { uint8_t buf[1] = { '9' }; Relay_Analysis(buf, MQTT_Mode); }
            else if (value == 0 && !allOff) { uint8_t buf[1] = { '0' }; Relay_Analysis(buf, MQTT_Mode); }
        }
    }
}

// ─── Singleton instance + free-function wrappers ─────────────────────────────

static MQTTManager mqttManager;

void MQTT_Init()                     { mqttManager.init(); }
void sendJsonData(JsonDocument& doc) { mqttManager.publish(doc); }


