#include <riot2/OrchestratorClient.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <riot2/ConfigCache.h>
#include <riot2/TlsRootCa.h>

namespace {

constexpr size_t kMaxConfigurationBytes = 32768;
constexpr size_t kReadBufferBytes = 256;
constexpr unsigned long kConfigurationReadTimeoutMs = 8000;

String buildConfigurationUrl(const String& apiBaseUrl, const String& nodeId) {
    String base = apiBaseUrl;
    if (!base.endsWith("/")) {
        base += "/";
    }
    return base + "api/Nodes/" + nodeId + "/configuration";
}

bool readConfigurationBody(HTTPClient& http, String& body) {
    int contentLength = http.getSize();
    if (contentLength > static_cast<int>(kMaxConfigurationBytes)) {
        Serial.printf("[Orchestrator] Configuration response too large: %d bytes (max=%u)\n", contentLength,
                      static_cast<unsigned>(kMaxConfigurationBytes));
        return false;
    }

    if (contentLength > 0) {
        body.reserve(static_cast<unsigned int>(contentLength));
    } else {
        body.reserve(4096);
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[kReadBufferBytes];
    size_t total = 0;
    unsigned long lastProgressMs = millis();

    while (http.connected() && (contentLength < 0 || total < static_cast<size_t>(contentLength))) {
        int available = stream->available();
        if (available <= 0) {
            if ((millis() - lastProgressMs) >= kConfigurationReadTimeoutMs) {
                Serial.println("[Orchestrator] Configuration response timed out while reading body");
                return false;
            }
            delay(1);
            continue;
        }

        size_t toRead = static_cast<size_t>(available);
        if (toRead > sizeof(buffer)) {
            toRead = sizeof(buffer);
        }
        if (contentLength >= 0) {
            size_t remaining = static_cast<size_t>(contentLength) - total;
            if (toRead > remaining) {
                toRead = remaining;
            }
        }
        if (total + toRead > kMaxConfigurationBytes) {
            Serial.printf("[Orchestrator] Configuration response exceeded max size (%u bytes)\n",
                          static_cast<unsigned>(kMaxConfigurationBytes));
            return false;
        }

        size_t read = stream->readBytes(buffer, toRead);
        if (read == 0) {
            continue;
        }
        body.concat(reinterpret_cast<const char*>(buffer), read);
        total += read;
        lastProgressMs = millis();
    }

    if (contentLength >= 0 && total != static_cast<size_t>(contentLength)) {
        Serial.printf("[Orchestrator] Configuration response ended early: %u/%d bytes\n", static_cast<unsigned>(total),
                      contentLength);
        return false;
    }

    return true;
}

}  // namespace

// Out-of-line definition required pre-C++17 whenever a static constexpr
// member is ODR-used (see repo memory notes on ProvisioningPortal::kDnsPort).
constexpr uint32_t OrchestratorClient::kHttpTimeoutMs;

bool OrchestratorClient::loadCached() {
    if (!_cacheEnabled) {
        return false;
    }

    ConfigCache::begin();

    String json;
    if (!ConfigCache::tryLoad(json)) {
        return false;
    }

    NodeConfiguration parsed;
    if (!parseConfiguration(json, parsed)) {
        Serial.println("[Orchestrator] Cached configuration failed to parse, ignoring");
        return false;
    }

    _current = parsed;
    _lastAppliedJson = json;
    Serial.printf("[Orchestrator] Loaded cached configuration from flash: %u device configuration(s)\n",
                  static_cast<unsigned>(_current.deviceConfigurations.size()));

    if (_callback) {
        _callback(_current);
    }

    return true;
}

bool OrchestratorClient::requestConfiguration(const String& apiBaseUrl, const String& nodeId) {
    String url = buildConfigurationUrl(apiBaseUrl, nodeId);
    Serial.printf("[Orchestrator] Fetching configuration from %s\n", url.c_str());

    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);

    // The orchestrator may be reachable over plain HTTP (typical on a local
    // network) or HTTPS; HTTPS is validated against the configured root CA
    // (see TlsRootCa.h), falling back to an insecure connection with a
    // logged warning if none is configured.
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    bool began;
    if (url.startsWith("https://")) {
        const char* pem = riot2::rootCaPem();
        if (pem[0] != '\0') {
            secureClient.setCACert(pem);
        } else {
            Serial.println(
                "[Orchestrator] WARNING: no root CA configured (RIOT2_ROOT_CA_PEM), HTTPS certificate will NOT "
                "be validated");
            secureClient.setInsecure();
        }
        began = http.begin(secureClient, url);
    } else {
        began = http.begin(plainClient, url);
    }

    if (!began) {
        Serial.println("[Orchestrator] Failed to start HTTP request");
        return false;
    }

    int statusCode = http.GET();
    if (statusCode != HTTP_CODE_OK) {
        Serial.printf("[Orchestrator] GET failed, status=%d\n", statusCode);
        http.end();
        return false;
    }

    String body;
    if (!readConfigurationBody(http, body)) {
        http.end();
        return false;
    }
    http.end();

    if (_cacheEnabled) {
        ConfigCache::save(body);
    }

    // See the doc comment on requestConfiguration() in the header - the
    // orchestrator can re-deliver the exact same configuration more than
    // once (e.g. a retained MQTT configuration message re-triggering this
    // on every reconnect), and re-applying it means a full, expensive
    // ViewManager rebuild (tear down + recreate every view's LVGL widgets)
    // for no actual change - measured to spike free heap from ~25KB down to
    // ~6KB on a 13-view configuration when it happened twice back-to-back.
    if (body == _lastAppliedJson) {
        Serial.println("[Orchestrator] Fetched configuration is unchanged, skipping rebuild");
        return true;
    }

    NodeConfiguration parsed;
    if (!parseConfiguration(body, parsed)) {
        Serial.println("[Orchestrator] Failed to parse configuration JSON");
        return false;
    }

    _current = parsed;
    _lastAppliedJson = body;
    Serial.printf("[Orchestrator] Configuration updated: %u device configuration(s)\n",
                  static_cast<unsigned>(_current.deviceConfigurations.size()));

    if (_callback) {
        _callback(_current);
    }

    return true;
}

bool OrchestratorClient::parseConfiguration(const String& json, NodeConfiguration& out) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.printf("[Orchestrator] JSON parse error: %s\n", error.c_str());
        return false;
    }

    out.name = doc["name"] | "";
    out.id = doc["id"] | "";
    out.deviceConfigurations.clear();

    // Both on-screen Views and non-visual Grove-port peripherals (see
    // IPeripheral/PeripheralManager) are entries in this same array,
    // distinguished purely by classFullName - ViewManager and
    // PeripheralManager each resolve/keep only the entries their own
    // factory recognizes (see DeviceConfiguration.h).
    for (JsonObject deviceJson : doc["deviceConfigurations"].as<JsonArray>()) {
        DeviceConfiguration device;
        device.id = deviceJson["id"] | "";
        device.name = deviceJson["name"] | "";
        device.classFullName = deviceJson["classFullName"] | "";

        for (JsonObject cmdJson : deviceJson["commandTemplates"].as<JsonArray>()) {
            CommandTemplate cmd;
            cmd.id = cmdJson["id"] | "";
            cmd.type = cmdJson["type"] | "";
            cmd.name = cmdJson["name"] | "";
            cmd.address = cmdJson["address"] | "";
            cmd.valueType = cmdJson["valueType"] | 0;
            cmd.model = cmdJson["model"] | false;
            device.commandTemplates.push_back(cmd);
        }

        for (JsonObject reportJson : deviceJson["reportTemplates"].as<JsonArray>()) {
            ReportTemplate report;
            report.id = reportJson["id"] | "";
            report.type = reportJson["type"] | "";
            report.name = reportJson["name"] | "";
            report.address = reportJson["address"] | "";
            for (JsonPair kv : reportJson["parameters"].as<JsonObject>()) {
                report.parameters.push_back({String(kv.key().c_str()), kv.value().as<String>()});
            }
            device.reportTemplates.push_back(report);
        }

        for (JsonPair kv : deviceJson["deviceParameters"].as<JsonObject>()) {
            device.deviceParameters.push_back({String(kv.key().c_str()), kv.value().as<String>()});
        }

        out.deviceConfigurations.push_back(device);
    }

    return true;
}
