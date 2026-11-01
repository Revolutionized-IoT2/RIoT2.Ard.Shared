#pragma once

#include <Arduino.h>

#include <functional>
#include <vector>

#include "riot2/BleTypes.h"
#include "riot2/Command.h"
#include "riot2/CommandResults.h"
#include "riot2/ConfigTemplateServer.h"
#include "riot2/ConfigurationRetry.h"
#include "riot2/DeviceConfiguration.h"
#include "riot2/MqttConnection.h"
#include "riot2/NodeConfig.h"
#include "riot2/NodeStatus.h"
#include "riot2/OrchestratorClient.h"
#include "riot2/PeripheralManager.h"
#include "riot2/ProvisioningPortal.h"
#include "riot2/Report.h"
#include "riot2/WifiConnection.h"

namespace riot2 {

// What a board tells the runtime about its views and UI. Every hook is optional.
struct NodeRuntimeHooks {
    // The board's view templates; served at /api/device/configuration/templates together with
    // the registered peripherals' templates.
    std::function<std::vector<DeviceConfiguration>()> viewTemplates;
    // True for a classFullName the board shows as a view, so PeripheralManager skips it.
    std::function<bool(const String& classFullName)> isView;
    // A fetched configuration, before the peripherals are rebuilt. Rebuild the views and return
    // true when a view consumes BLE events: the runtime then starts the BLE scanner.
    std::function<bool(const NodeConfiguration&)> onConfiguration;
    // A command for the views; return true when a view has its id. system.ota never gets here;
    // peripherals get every command too. A command no view or peripheral has is answered
    // `unknownCommand` (design 7.1).
    std::function<bool(const Command&)> onCommand;
    // The orchestrator announced a configuration; the fetch runs from loop().
    std::function<void()> onConfigurationRequested;
    // system.ota: about to download `url` (the device restarts on success), or the update failed.
    std::function<void(const String& url)> onOtaStarted;
    std::function<void()> onOtaFailed;
    // The BLE scanner just started (once, after a configuration asked for it).
    std::function<void()> onBleStarted;
    std::function<void(const BleDeviceInfo&)> onBleDeviceDiscovered;
    std::function<void(const String& address)> onBleDeviceLost;
    std::function<void(const BleAdvertisement&)> onBleAdvertisement;
};

// The node lifecycle both firmwares share (ADR 0011, plan M5 step 3): NVS settings, the
// provisioning portal, Wi-Fi, MQTT and the orchestrator handshake, the configuration fetch with
// retry, the template server, SNTP, OTA (system.ota and the rollback trial), Grove peripherals and
// the BLE scanner. Since firmware 0.5.0 it also answers correlated commands with a result and
// publishes the retained configuration status, and advertises both (designs 7.1 and 7.2).
// A board's main.cpp sets up its hardware and views, calls begin() and loop(), and reacts through
// NodeRuntimeHooks.
class NodeRuntime {
public:
    enum class Mode { Provisioning, Normal };

    // Call first in setup(), before anything that can crash: starts the trial of a new OTA image
    // (riot2/OtaRollback.h). The first successful configuration fetch ends it.
    static void startOtaTrial();

    // Loads the NodeConfig from NVS. Without a valid one, starts the provisioning portal
    // (isProvisioning()); otherwise starts Wi-Fi, MQTT and the template server.
    void begin(const char* manifestJson, NodeRuntimeHooks hooks);
    void loop();

    Mode mode() const { return _mode; }
    bool isProvisioning() const { return _mode == Mode::Provisioning; }
    const NodeConfig& config() const { return _config; }
    WifiConnection& wifi() { return _wifi; }
    MqttConnection& mqtt() { return _mqtt; }
    const ProvisioningPortal& provisioning() const { return _provisioning; }
    bool bleActive() const { return _bleActive; }
    // The node's view of its configuration, as published on riot2/node/{id}/status.
    const riot2::NodeStatus& configurationStatus() const { return _status; }

    // For the views' reports.
    void publishReport(const Report& report);

    // Publishes the offline presence and disconnects, as before a power-off.
    void publishOfflineAndDisconnect();

    // Publishes the offline presence, clears the NodeConfig and restarts into provisioning.
    void factoryReset();

private:
    Mode _mode = Mode::Normal;
    NodeConfig _config;
    NodeRuntimeHooks _hooks;
    WifiConnection _wifi;
    MqttConnection _mqtt;
    ProvisioningPortal _provisioning;
    ConfigTemplateServer _templateServer;
    // No configuration cache: without a live orchestrator the node shows its start screen rather
    // than a stale UI (see OrchestratorClient.h).
    OrchestratorClient _orchestrator{false};
    // Grove-port peripherals, configured like views but never shown (PeripheralManager.h).
    PeripheralManager _peripherals;
    // The configuration fetch is a blocking HTTP GET. Run inside PubSubClient's callback it starved
    // the MQTT keepalive and dropped the connection, which made the orchestrator resend the
    // configuration in a loop, so the callback only records the request and loop() fetches.
    ConfigurationRetry _configurationRetry;
    // Started once, when a configuration first has a BLE-consuming view; never stopped.
    bool _bleActive = false;
    // Design 7.2: published retained on riot2/node/{id}/status.
    NodeStatus _status;
    // The configuration's devices whose class is neither a view nor a peripheral of this build.
    std::vector<FailedDevice> _failedDevices;
    // The hash of the configuration command the pending fetch is for; empty from an orchestrator
    // before 1.5.0.
    String _pendingHash;
    // Design 7.1: the outcomes of the last 16 correlated commands, for redeliveries.
    CorrelationRing _correlations;

    void handleCommand(const String& topic, const String& payload);
    void handleOrchestratorOnline(const String& topic, const String& payload);
    void handleConfigurationMessage(const String& topic, const String& payload);
    void handleConfiguration(const NodeConfiguration& configuration);
    void configurationFetched(bool fetched);
    void publishStatus();
    // Remembers the outcome of a correlated command for redeliveries, then publishes it.
    void completeCommand(const String& correlationId, const String& commandId, const char* status,
                         const String& error);
    // No-op without a correlationId: an orchestrator before 1.4.0 expects no result.
    void publishCommandResult(const String& correlationId, const String& commandId, const char* status,
                              const String& error);
};

}  // namespace riot2
