// Host stand-ins for the NodeRuntime members that its MQTT and configuration handlers use, shared by
// golden_messages.cpp and desired_state.cpp. The Python tests (runtime_source() in
// test_golden_messages.py) paste this file over the RUNTIME_HARNESS marker, the real
// CommandResults.cpp and NodeStatus.cpp over PRODUCTION_SOURCES, and the production handler bodies
// over PRODUCTION_HANDLERS.
// Compiled with tests/host (Arduino.h) and RIoT2Shared/include on the include path.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

unsigned long hostMillis = 0;
struct Log {
    template <typename... Args> void printf(const char*, Args...) {}
    void println(const char*) {}
} Serial;

#include <riot2/Command.h>
#include <riot2/CommandResults.h>
#include <riot2/ConfigurationRetry.h>
#include <riot2/DeviceConfiguration.h>
#include <riot2/NodeStatus.h>
#include <riot2/Topics.h>

// PRODUCTION_SOURCES

namespace riot2 {
const char* const kOtaCommandId = "system.ota";
std::vector<String> savedOtaCommands;
void saveInstalledOtaCommand(const String& correlationId) {
    if (correlationId.length() > 0) savedOtaCommands.push_back(correlationId);
}
}  // namespace riot2

namespace OtaUpdater {
bool succeeds = false;
std::vector<String> urls;
// The real one restarts after onInstalled and never returns true.
bool performUpdate(const String& url, const std::function<void()>& onInstalled) {
    urls.push_back(url);
    if (!succeeds) return false;
    if (onInstalled) onInstalled();
    return true;
}
}  // namespace OtaUpdater

struct PeripheralFactory {
    static PeripheralFactory& instance() {
        static PeripheralFactory factory;
        return factory;
    }
    bool isRegistered(const String& classFullName) const { return classFullName == "RIoT2.Test.Peripheral"; }
};

struct BleScanner {
    unsigned begins = 0;
    static BleScanner& instance() {
        static BleScanner scanner;
        return scanner;
    }
    void begin() { ++begins; }
};

struct Published {
    String topic;
    String payload;
    bool retained;
};

struct FakeMqtt {
    std::vector<Published> published;
    size_t packet = 512;
    bool publishJson(const String& topic, const JsonDocument& doc, bool retained) {
        std::string payload;
        serializeJson(doc, payload);
        published.push_back({topic, String(payload), retained});
        return true;
    }
    size_t maxPayloadBytes(const String& topic) { return packet - 7 - topic.length(); }
};

// OrchestratorClient after a fetch: the configuration it holds and why the last fetch failed.
struct FakeOrchestrator {
    NodeConfiguration configuration;
    String error;
    const NodeConfiguration& current() const { return configuration; }
    const String& lastError() const { return error; }
};

struct FakeWifi {
    bool modemSleep = false;
    void setModemSleepEnabled(bool enabled) { modemSleep = enabled; }
};

// PeripheralManager: owns the command ids in `owned`.
struct FakePeripherals {
    unsigned calls = 0;
    unsigned rebuilds = 0;
    String lastId;
    std::vector<String> owned;
    bool onCommand(const String& commandId, const Command&) {
        ++calls;
        lastId = commandId;
        for (const auto& id : owned) {
            if (id == commandId) return true;
        }
        return false;
    }
    void rebuild(const NodeConfiguration&, const std::function<bool(const String&)>&) { ++rebuilds; }
};

struct FakeHooks {
    std::function<bool(const String&)> isView;
    std::function<bool(const NodeConfiguration&)> onConfiguration;
    std::function<bool(const Command&)> onCommand;
    std::function<void()> onConfigurationRequested;
    std::function<void(const String&)> onOtaStarted;
    std::function<void()> onOtaFailed;
    std::function<void()> onBleStarted;
};

namespace riot2 {
struct NodeRuntime {
    struct {
        String id = "golden";
    } _config;
    FakeHooks _hooks;
    FakePeripherals _peripherals;
    ConfigurationRetry _configurationRetry;
    FakeMqtt _mqtt;
    FakeOrchestrator _orchestrator;
    FakeWifi _wifi;
    bool _bleActive = false;
    NodeStatus _status;
    std::vector<FailedDevice> _failedDevices;
    String _pendingHash;
    CorrelationRing _correlations;

    void handleCommand(const String& topic, const String& payload);
    void handleConfigurationMessage(const String& topic, const String& payload);
    void handleConfiguration(const NodeConfiguration& configuration);
    void configurationFetched(bool fetched);
    void publishStatus();
    void completeCommand(const String& correlationId, const String& commandId, const char* status,
                         const String& error);
    void publishCommandResult(const String& correlationId, const String& commandId, const char* status,
                              const String& error);

    // NodeRuntime::loop()'s fetch step; `fetch` plays OrchestratorClient::requestConfiguration().
    template <typename Fetch>
    void runFetch(Fetch fetch) {
        _configurationRetry.loop(true, [&](const String& url) {
            bool fetched = fetch(url);
            configurationFetched(fetched);
            return fetched;
        });
    }
};

// PRODUCTION_HANDLERS
}  // namespace riot2
