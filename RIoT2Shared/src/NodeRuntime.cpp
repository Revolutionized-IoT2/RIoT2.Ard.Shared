#include "riot2/NodeRuntime.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#include "riot2/BleScanner.h"
#include "riot2/OtaRollback.h"
#include "riot2/OtaUpdater.h"
#include "riot2/PeripheralFactory.h"
#include "riot2/Topics.h"

namespace riot2 {

namespace {
// Reserved command id (a RIoT2.Ard extension, not part of RIoT2.Core's Command contract) that
// updates the firmware instead of reaching a view: { "id": "system.ota", "value": "<firmware url>" }.
const char* const kOtaCommandId = "system.ota";

// The correlationId of the system.ota command whose image was just installed, kept in NodeConfig's
// NVS namespace across the restart. Without it, a result lost while the download blocked MQTT
// would make the orchestrator deliver the command again, and the node would install it twice.
const char* const kNvsNamespace = "riot2node";
const char* const kOtaCorrelationKey = "otaCorr";

void saveInstalledOtaCommand(const String& correlationId) {
    if (correlationId.length() == 0) return;
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    prefs.putString(kOtaCorrelationKey, correlationId);
    prefs.end();
}

String takeInstalledOtaCommand() {
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    String correlationId;
    // isKey() first: getString() logs an error for a missing key, which is the normal case.
    if (prefs.isKey(kOtaCorrelationKey)) {
        correlationId = prefs.getString(kOtaCorrelationKey, "");
        prefs.remove(kOtaCorrelationKey);
    }
    prefs.end();
    return correlationId;
}
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

    String installedOta = takeInstalledOtaCommand();
    if (installedOta.length() > 0) {
        _correlations.remember(installedOta, CommandResultStatus::Executed, String());
    }

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
    _mqtt.onConnected([this]() { publishStatus(); });
    _mqtt.setCapabilities({Capabilities::CommandResult, Capabilities::DesiredState});
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
        configurationFetched(fetched);
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

    // Design 7.1: a command with a correlationId gets a result, and a redelivery is answered from
    // the ring without running the command again.
    String correlationId = doc["correlationId"] | "";
    const CorrelationRing::Entry* seen = _correlations.find(correlationId);
    if (seen) {
        Serial.printf("[Command] %s (%s) was received before, not running it again\n", id.c_str(),
                      correlationId.c_str());
        publishCommandResult(correlationId, id, CorrelationRing::redeliveryStatus(*seen), seen->error);
        return;
    }

    if (id == kOtaCommandId) {
        String url = doc["value"] | "";
        Serial.printf("[OTA] Update requested: %s\n", url.c_str());
        if (_hooks.onOtaStarted) _hooks.onOtaStarted(url);
        bool installed = OtaUpdater::performUpdate(url, [this, &correlationId, &id]() {
            saveInstalledOtaCommand(correlationId);
            completeCommand(correlationId, id, CommandResultStatus::Executed, String());
        });
        if (!installed) {
            if (_hooks.onOtaFailed) _hooks.onOtaFailed();
            completeCommand(correlationId, id, CommandResultStatus::Failed,
                            "The firmware update failed; the node's serial log has the reason.");
        }
        return;
    }

    Command command{id, doc["value"]};
    // Command template ids are unique per configuration, so exactly one of the views and the
    // peripherals owns a given id; both are asked.
    bool handled = _hooks.onCommand && _hooks.onCommand(command);
    handled = _peripherals.onCommand(command.id, command) || handled;
    if (handled) {
        completeCommand(correlationId, id, CommandResultStatus::Executed, String());
        return;
    }

    if (!_status.hasAppliedConfiguration() || _configurationRetry.pending()) {
        // Before the first configuration and while a new one is fetched, an unknown command is a
        // matter of time: rejected and not remembered, so the orchestrator's next delivery runs it.
        Serial.printf("[Command] %s rejected until the configuration is applied\n", id.c_str());
        publishCommandResult(correlationId, id, CommandResultStatus::Rejected,
                             "The node is still applying its configuration.");
        return;
    }
    Serial.printf("[Command] %s has no view or peripheral on this node\n", id.c_str());
    completeCommand(correlationId, id, CommandResultStatus::UnknownCommand,
                    "No view or peripheral on this node has command " + id + ".");
}

void NodeRuntime::completeCommand(const String& correlationId, const String& commandId, const char* status,
                                  const String& error) {
    _correlations.remember(correlationId, status, error);
    publishCommandResult(correlationId, commandId, status, error);
}

void NodeRuntime::publishCommandResult(const String& correlationId, const String& commandId, const char* status,
                                       const String& error) {
    if (correlationId.length() == 0) return;
    JsonDocument doc;
    writeCommandResult(doc, correlationId, commandId, status, error, static_cast<uint32_t>(time(nullptr)));
    _mqtt.publishJson(Topics::commandResult(_config.id), doc, false);
}

void NodeRuntime::publishStatus() {
    String topic = Topics::status(_config.id);
    JsonDocument doc;
    _status.write(doc, static_cast<uint32_t>(time(nullptr)), _mqtt.maxPayloadBytes(topic));
    _mqtt.publishJson(topic, doc, true);
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

    // Design 7.2: the orchestrator sends this on every presence and save, and the node decides.
    // The applied hash costs one status message and no fetch; a command without a hash (an
    // orchestrator before 1.5.0) is always fetched.
    String hash = doc["hash"] | "";
    if (_status.isApplied(hash)) {
        Serial.println("[Orchestrator] Configuration is already applied, not fetching");
        _configurationRetry.cancel();
        publishStatus();
        return;
    }
    if (hash.length() > 0 && hash == _pendingHash && _configurationRetry.pending()) {
        // Already being fetched: a repeat must not reset the retry backoff.
        return;
    }

    _pendingHash = hash;
    _configurationRetry.request(apiBaseUrl);
    if (_hooks.onConfigurationRequested) _hooks.onConfigurationRequested();
}

void NodeRuntime::configurationFetched(bool fetched) {
    if (fetched) {
        // Also after a 304 or an unchanged body: the configuration is applied again, and a
        // failed state from an earlier attempt ends.
        const NodeConfiguration& current = _orchestrator.current();
        _status.applied(current.revision, current.hash, _failedDevices);
        publishStatus();
        return;
    }
    if (_status.failed(_orchestrator.lastError())) {
        publishStatus();
    }
}

void NodeRuntime::handleConfiguration(const NodeConfiguration& configuration) {
    _failedDevices.clear();
    for (const auto& device : configuration.deviceConfigurations) {
        Serial.printf("[Orchestrator]   device id=%s name=%s classFullName=%s (%u commands, %u reports)\n",
                      device.id.c_str(), device.name.c_str(), device.classFullName.c_str(),
                      static_cast<unsigned>(device.commandTemplates.size()),
                      static_cast<unsigned>(device.reportTemplates.size()));
        bool known = (_hooks.isView && _hooks.isView(device.classFullName)) ||
                     PeripheralFactory::instance().isRegistered(device.classFullName);
        if (!known) {
            _failedDevices.push_back({device.id, "Unknown class"});
        }
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
