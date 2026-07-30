#include <riot2/OrchestratorClient.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <riot2/ConfigCache.h>
#include <riot2/TlsRootCa.h>

namespace {

String buildConfigurationUrl(const String& apiBaseUrl, const String& nodeId) {
    String base = apiBaseUrl;
    if (!base.endsWith("/")) {
        base += "/";
    }
    return base + "api/Nodes/" + nodeId + "/configuration";
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

    String body = http.getString();
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
