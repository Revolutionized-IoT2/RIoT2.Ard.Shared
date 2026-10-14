#include "riot2/NodeRuntime.h"

#include <ArduinoJson.h>
#include <time.h>

#include "riot2/BleScanner.h"
#include "riot2/OtaRollback.h"
#include "riot2/OtaUpdater.h"
#include "riot2/PeripheralFactory.h"

namespace riot2 {

namespace {
// Reserved command id (a RIoT2.Ard extension, not part of RIoT2.Core's Command contract) that
// updates the firmware instead of reaching a view: { "id": "system.ota", "value": "<firmware url>" }.
const char* const kOtaCommandId = "system.ota";
}  // namespace

void NodeRuntime::startOtaTrial() {
    OtaRollback::begin();
}

void NodeRuntime::begin(const char* manifestJson, NodeRuntimeHooks hooks) {
    _hooks = std::move(hooks);
    _config = NodeConfigStore::load();

    if (!_config.isValid()) {
        Serial.println("[Config] No valid NodeConfig in NVS. Starting provisioning portal.");
        _mode = Mode::Provisioning;
        _provisioning.begin();
        return;
    }
    _mode = Mode::Normal;
    Serial.printf("[Config] Loaded node id=%s name=%s\n", _config.id.c_str(), _config.name.c_str());

    // Wired before the first fetch can run, so a fetch never fires into an unset callback.
    _peripherals.onReport([this](const Report& report) { publishReport(report); });
    _orchestrator.onConfigurationUpdated([this](const NodeConfiguration& configuration) { handleConfiguration(configuration); });

    // The radio itself starts only once a configuration has a BLE-consuming view.
    BleScanner::instance().onDeviceDiscovered([this](const BleDeviceInfo& device) {
        if (_hooks.onBleDeviceDiscovered) _hooks.onBleDeviceDiscovered(device);
    });
    BleScanner::instance().onDeviceLost([this](const String& address) {
        if (_hooks.onBleDeviceLost) _hooks.onBleDeviceLost(address);
    });
    BleScanner::instance().onAdvertisement([this](const BleAdvertisement& advertisement) {
        if (_hooks.onBleAdvertisement) _hooks.onBleAdvertisement(advertisement);
    });

    _wifi.begin(_config.wifiSsid, _config.wifiPassword);

    // Served as soon as Wi-Fi is up, so the orchestrator can read the templates from the
    // nodeBaseUrl in the presence message as soon as it sees the node.
    _templateServer.begin([this]() {
        std::vector<DeviceConfiguration> templates;
        if (_hooks.viewTemplates) templates = _hooks.viewTemplates();
        std::vector<DeviceConfiguration> peripherals = PeripheralFactory::instance().configurationTemplates();
        templates.insert(templates.end(), peripherals.begin(), peripherals.end());
        return templates;
    });

    // SNTP, so Report.timeStamp is a Unix epoch once Wi-Fi is up. Reports before the first sync
    // carry a small timestamp.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    _mqtt.onCommand([this](const String& topic, const String& payload) { handleCommand(topic, payload); });
    _mqtt.onOrchestratorOnline([this](const String& topic, const String& payload) { handleOrchestratorOnline(topic, payload); });
    _mqtt.onConfigurationMessage([this](const String& topic, const String& payload) { handleConfigurationMessage(topic, payload); });
    _mqtt.setManifestJson(manifestJson);
    _mqtt.begin(_config);
}

void NodeRuntime::loop() {
    if (_mode == Mode::Provisioning) {
        _provisioning.loop();
        return;
    }

    _wifi.loop();
    _mqtt.loop();
    _templateServer.loop();

    // The fetch requested by handleConfigurationMessage(), outside mqtt.loop()'s callback; failed
    // attempts retry with bounded backoff while Wi-Fi is up. A successful fetch (Wi-Fi, MQTT and
    // the orchestrator all work, and the views were rebuilt) is what keeps a new OTA image.
    _configurationRetry.loop(_wifi.isConnected(), [this](const String& url) {
        bool fetched = _orchestrator.requestConfiguration(url, _config.id);
        if (fetched) {
            OtaRollback::markHealthy();
        }
        return fetched;
    });

    _peripherals.loop();

    if (_bleActive) {
        BleScanner::instance().loop();
    }
}

void NodeRuntime::publishReport(const Report& report) {
    Serial.printf("[Report] id=%s value=%s\n", report.id.c_str(), report.value.c_str());
    _mqtt.publishReport(report);
}

void NodeRuntime::publishOfflineAndDisconnect() {
    _mqtt.publishOfflineAndDisconnect();
}

void NodeRuntime::factoryReset() {
    Serial.println("[Config] Factory reset requested, clearing NodeConfig");
    _mqtt.publishOfflineAndDisconnect();
    NodeConfigStore::clear();
    delay(500);
    ESP.restart();
}

void NodeRuntime::handleCommand(const String& topic, const String& payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[Command] Failed to parse %s: %s\n", topic.c_str(), error.c_str());
        return;
    }

    String id = doc["id"] | "";
    if (id.length() == 0) {
        Serial.printf("[Command] %s missing id, ignoring\n", topic.c_str());
        return;
    }

    if (id == kOtaCommandId) {
        String url = doc["value"] | "";
        Serial.printf("[OTA] Update requested: %s\n", url.c_str());
        if (_hooks.onOtaStarted) _hooks.onOtaStarted(url);
        if (!OtaUpdater::performUpdate(url) && _hooks.onOtaFailed) {
            _hooks.onOtaFailed();
        }
        return;
    }

    Command command{id, doc["value"]};
    // Command template ids are unique per configuration, so exactly one of the views and the
    // peripherals owns a given id; both are asked.
    if (_hooks.onCommand) _hooks.onCommand(command);
    _peripherals.onCommand(command.id, command);
}

void NodeRuntime::handleOrchestratorOnline(const String& topic, const String& payload) {
    (void)topic;
    (void)payload;
    // riot2/orchestrator/online carries nothing the node needs: it announces itself again, and
    // the orchestrator answers on riot2/node/{id}/configuration.
    Serial.println("[Orchestrator] Orchestrator online, re-announcing node");
    _mqtt.publishOnline();
}

void NodeRuntime::handleConfigurationMessage(const String& topic, const String& payload) {
    (void)topic;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[Orchestrator] Failed to parse configuration message: %s\n", error.c_str());
        return;
    }

    String apiBaseUrl = doc["apiBaseUrl"] | "";
    if (apiBaseUrl.length() == 0) {
        Serial.println("[Orchestrator] configuration message missing apiBaseUrl");
        return;
    }

    _configurationRetry.request(apiBaseUrl);
    if (_hooks.onConfigurationRequested) _hooks.onConfigurationRequested();
}

void NodeRuntime::handleConfiguration(const NodeConfiguration& configuration) {
    for (const auto& device : configuration.deviceConfigurations) {
        Serial.printf("[Orchestrator]   device id=%s name=%s classFullName=%s (%u commands, %u reports)\n",
                      device.id.c_str(), device.name.c_str(), device.classFullName.c_str(),
                      static_cast<unsigned>(device.commandTemplates.size()),
                      static_cast<unsigned>(device.reportTemplates.size()));
    }
    bool wantsBle = _hooks.onConfiguration && _hooks.onConfiguration(configuration);
    _peripherals.rebuild(configuration, _hooks.isView);

    if (!wantsBle) {
        return;
    }
    // Before BleScanner::begin(), and kept for later Wi-Fi reconnects: the ESP32 Wi-Fi/BT
    // coexistence layer aborts (reboots) when modem sleep is off while the BLE radio runs - see
    // WifiConnection::setModemSleepEnabled().
    _wifi.setModemSleepEnabled(true);
    if (!_bleActive) {
        Serial.println("[BLE] Configuration includes a BLE-consuming view, enabling the BLE scanner");
        _bleActive = true;
        BleScanner::instance().begin();
        if (_hooks.onBleStarted) _hooks.onBleStarted();
    }
}

}  // namespace riot2
