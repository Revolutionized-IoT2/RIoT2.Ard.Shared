#include <riot2/MqttConnection.h>

#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

#include <riot2/TlsRootCa.h>
#include <riot2/Topics.h>
#include <riot2/MqttJson.h>

MqttConnection* MqttConnection::_instance = nullptr;

namespace {

void publishDocument(PubSubClient& client, const String& topic, const JsonDocument& doc, bool retained) {
    auto result = riot2::publishJson<MQTT_MAX_PACKET_SIZE>(client, topic.c_str(), doc, retained);
    if (result != riot2::JsonPublishResult::Published) {
        Serial.printf("[MQTT] JSON publish failed (reason=%u, bytes=%u, packetBuffer=%u)\n",
                      static_cast<unsigned>(result), static_cast<unsigned>(measureJson(doc)),
                      static_cast<unsigned>(client.getBufferSize()));
    }
}

// Splits "[scheme://]host[:port]" into host/port, defaulting the port to
// defaultPort when the URL doesn't specify one.
bool parseMqttUrl(const String& url, String& host, uint16_t& port, uint16_t defaultPort) {
    String remainder = url;

    int schemeIdx = remainder.indexOf("://");
    if (schemeIdx >= 0) {
        remainder = remainder.substring(schemeIdx + 3);
    }

    int slashIdx = remainder.indexOf('/');
    if (slashIdx >= 0) {
        remainder = remainder.substring(0, slashIdx);
    }

    int colonIdx = remainder.indexOf(':');
    if (colonIdx >= 0) {
        host = remainder.substring(0, colonIdx);
        String portText = remainder.substring(colonIdx + 1);
        host.trim();
        portText.trim();
        if (portText.length() == 0) {
            return false;
        }
        uint32_t parsedPort = 0;
        for (size_t i = 0; i < portText.length(); ++i) {
            char c = portText[i];
            if (c < '0' || c > '9') {
                return false;
            }
            parsedPort = parsedPort * 10 + static_cast<uint32_t>(c - '0');
            if (parsedPort > 65535) {
                return false;
            }
        }
        if (parsedPort == 0) {
            return false;
        }
        port = static_cast<uint16_t>(parsedPort);
    } else {
        host = remainder;
        host.trim();
        port = defaultPort;
    }

    return host.length() > 0;
}

}  // namespace

void MqttConnection::begin(const NodeConfig& config) {
    _config = config;
    _instance = this;

    _serverConfigured = parseMqttUrl(config.mqttServerUrl, _brokerHost, _brokerPort, config.mqttUseTls ? 8883 : 1883);
    if (!_serverConfigured) {
        Serial.printf("[MQTT] Invalid broker URL \"%s\"; MQTT will stay disconnected\n", config.mqttServerUrl.c_str());
    }

    if (config.mqttUseTls) {
        const char* pem = riot2::rootCaPem();
        if (pem[0] != '\0') {
            _secureClient.setCACert(pem);
        } else {
            Serial.println(
                "[MQTT] WARNING: mqttUseTls is set but no root CA is configured (RIOT2_ROOT_CA_PEM), broker "
                "certificate will NOT be validated");
            _secureClient.setInsecure();
        }
        _client.setClient(_secureClient);
    } else {
        _client.setClient(_plainClient);
    }

    // _brokerHost is a member (not a local) because PubSubClient::setServer
    // keeps the raw const char* pointer rather than copying it; a local
    // String's buffer would be freed once begin() returns, leaving a
    // dangling pointer and silently-corrupted hostname on every reconnect.
    _client.setServer(_brokerHost.c_str(), _brokerPort);
    _client.setCallback(staticCallback);

    _state = MqttState::Disconnected;
    _backoffMs = kInitialBackoffMs;
    _lastAttemptMs = 0;  // forces an immediate connect attempt once Wi-Fi is up
}

void MqttConnection::attemptConnect() {
    if (!_serverConfigured) {
        return;
    }

    Serial.printf("[MQTT] Connecting to broker as \"%s\" (tls=%d)...\n", _config.id.c_str(), _config.mqttUseTls);
    _lastAttemptMs = millis();

    String onlineTopic = Topics::online(_config.id);

    JsonDocument lwtDoc;
    lwtDoc["isOnline"] = false;
    char lwtPayload[64];
    size_t lwtLen = serializeJson(lwtDoc, lwtPayload, sizeof(lwtPayload));

    bool connected = _client.connect(
        _config.id.c_str(),
        _config.mqttUsername.length() ? _config.mqttUsername.c_str() : nullptr,
        _config.mqttPassword.length() ? _config.mqttPassword.c_str() : nullptr,
        onlineTopic.c_str(),
        0,     // willQos
        true,  // willRetain
        lwtPayload,
        static_cast<int>(lwtLen));

    if (!connected) {
        Serial.printf("[MQTT] Connect failed, rc=%d\n", _client.state());
        _state = MqttState::Disconnected;
        _backoffMs = min(_backoffMs * 2, kMaxBackoffMs);
        return;
    }

    Serial.println("[MQTT] Connected");
    _state = MqttState::Connected;
    _backoffMs = kInitialBackoffMs;

    _client.subscribe(Topics::orchestratorOnline());
    _client.subscribe(Topics::configuration(_config.id).c_str());
    _client.subscribe(Topics::command(_config.id).c_str());

    publishOnline();
}

void MqttConnection::publishOnline() {
    JsonDocument doc;
    doc["name"] = _config.name;
    doc["isOnline"] = true;
    doc["nodeType"] = 1;  // RIoT2.Core.Enums.NodeType.Device

    // Lets the orchestrator reach this node's HTTP API (e.g.
    // /api/device/configuration/templates, served by ConfigTemplateServer)
    // without needing it configured separately - read fresh here (rather
    // than cached at begin()) so it stays correct across a DHCP lease
    // renewal/reconnect. Omitted if Wi-Fi doesn't have an IP yet.
    IPAddress localIp = WiFi.localIP();
    if (localIp != IPAddress(0, 0, 0, 0)) {
        doc["nodeBaseUrl"] = "http://" + localIp.toString();
    }

    // _manifestJson is this build's manifest.json, embedded as a string
    // constant by the consuming project at build time (see
    // generate_manifest.py) and handed in via setManifestJson(); parse it
    // so it's nested as a real JSON object rather than a re-quoted string.
    if (_manifestJson) {
        JsonDocument manifestDoc;
        DeserializationError manifestErr = deserializeJson(manifestDoc, _manifestJson);
        if (!manifestErr) {
            doc["manifest"] = manifestDoc.as<JsonVariant>();
        } else {
            Serial.printf("[MQTT] Failed to parse embedded manifest: %s\n", manifestErr.c_str());
        }
    }

    publishDocument(_client, Topics::online(_config.id), doc, true);
}

void MqttConnection::publishOfflineAndDisconnect() {
    if (!_client.connected()) {
        return;
    }

    JsonDocument doc;
    doc["isOnline"] = false;
    publishDocument(_client, Topics::online(_config.id), doc, true);

    _client.disconnect();
    _state = MqttState::Disconnected;
}

void MqttConnection::publishReport(const Report& report) {
    if (!_client.connected()) {
        return;
    }

    // report.value is already a raw JSON literal (bool/number/string/object);
    // parse it so it can be embedded as-is rather than re-quoted as a string.
    JsonDocument valueDoc;
    DeserializationError valueErr = deserializeJson(valueDoc, report.value);

    JsonDocument doc;
    doc["id"] = report.id;
    doc["timeStamp"] = static_cast<uint32_t>(time(nullptr));  // requires configTime() to have synced; see main.cpp
    if (!valueErr) {
        doc["value"] = valueDoc.as<JsonVariant>();
    } else {
        doc["value"] = report.value;
    }

    publishDocument(_client, Topics::report(_config.id), doc, false);
}

void MqttConnection::loop() {
    if (!_serverConfigured) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;  // WifiConnection owns Wi-Fi retry; wait for it to come back
    }

    if (_client.connected()) {
        _state = MqttState::Connected;
        _client.loop();
        return;
    }

    if (_state == MqttState::Connected) {
        Serial.println("[MQTT] Connection lost");
    }
    _state = MqttState::Disconnected;

    unsigned long now = millis();
    if (_lastAttemptMs == 0 || (now - _lastAttemptMs) >= _backoffMs) {
        attemptConnect();
    }
}

bool MqttConnection::isConnected() {
    return _client.connected();
}

void MqttConnection::handleMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);
    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += static_cast<char>(payload[i]);
    }

    if (topicStr == Topics::orchestratorOnline()) {
        if (_orchestratorCallback) {
            _orchestratorCallback(topicStr, payloadStr);
        }
    } else if (topicStr == Topics::configuration(_config.id)) {
        if (_configurationCallback) {
            _configurationCallback(topicStr, payloadStr);
        }
    } else if (_commandCallback) {
        _commandCallback(topicStr, payloadStr);
    }
}

void MqttConnection::staticCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->handleMessage(topic, payload, length);
    }
}
