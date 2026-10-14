// Golden-file contract test (I05, plan M7 step 3): NodeRuntime's real MQTT handlers (both boards
// use them since I18) and OrchestratorClient::parseConfiguration read the golden files from
// RIoT2.Core/contracts/golden.
// test_golden_messages.py injects the production code and the golden JSON, runs this program and
// compares its output with what Python's json module reads from the same files.
#include <cassert>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <ArduinoJson.h>
using String = std::string;
struct Log {
    template<typename... Args> void printf(const char*, Args...) {}
    void println(const char*) {}
} Serial;
// PRODUCTION_CONFIGURATION_TYPES
// PRODUCTION_COMMAND_TYPE
struct OrchestratorClient {
    bool parseConfiguration(const String& json, NodeConfiguration& out);
};
// PRODUCTION_PARSE_CONFIGURATION
namespace riot2 {
String serializeDeviceConfigurations(const std::vector<DeviceConfiguration>& configs);
}
// PRODUCTION_SERIALIZE_CONFIGURATIONS

// The NodeRuntime members the handlers use.
const char* const kOtaCommandId = "system.ota";
namespace OtaUpdater { bool performUpdate(const String&) { return true; } }
struct Router {
    unsigned calls = 0;
    String id, value;
    bool onCommand(const String& commandId, const Command& command) {
        ++calls;
        id = commandId;
        value.clear();
        serializeJson(command.value, value);
        return false;
    }
};
struct Hooks {
    std::function<void(const Command&)> onCommand;
    std::function<void()> onConfigurationRequested;
    std::function<void(const String&)> onOtaStarted;
    std::function<void()> onOtaFailed;
};
struct Retry { String url; void request(const String& apiBaseUrl) { url = apiBaseUrl; } };
struct NodeRuntime {
    Hooks _hooks;
    Router _peripherals;
    Retry _configurationRetry;
    void handleCommand(const String& topic, const String& payload);
    void handleConfigurationMessage(const String& topic, const String& payload);
};
// PRODUCTION_HANDLERS
// GOLDEN_DATA

String parameters(const ParameterList& list) {
    String text;
    for (const auto& kv : list) text += (text.empty() ? "" : ",") + kv.first + "=" + kv.second;
    return text;
}

int main() {
    NodeRuntime runtime;
    Router views;
    unsigned requested = 0;
    runtime._hooks.onCommand = [&](const Command& command) { views.onCommand(command.id, command); };
    runtime._hooks.onConfigurationRequested = [&] { ++requested; };
    for (const auto& golden : kGoldenCommands) {
        views.calls = runtime._peripherals.calls = 0;
        runtime.handleCommand("riot2/node/golden/command", golden);
        assert(views.calls == 1 && runtime._peripherals.calls == 1);
        std::printf("command|%s|%s\n", views.id.c_str(), views.value.c_str());
    }

    runtime.handleConfigurationMessage("riot2/node/golden/configuration", kGoldenConfigurationCommand);
    assert(requested == 1);
    std::printf("configuration|%s\n", runtime._configurationRetry.url.c_str());

    OrchestratorClient client;
    NodeConfiguration node;
    assert(client.parseConfiguration(kGoldenNodeConfiguration, node));
    std::printf("node|%s|%s\n", node.id.c_str(), node.name.c_str());
    for (const auto& device : node.deviceConfigurations) {
        std::printf("device|%s|%s|%s|%s\n", device.id.c_str(), device.name.c_str(),
                    device.classFullName.c_str(), parameters(device.deviceParameters).c_str());
        for (const auto& command : device.commandTemplates)
            std::printf("  command|%s|%s|%s|model=%d|type=%d\n", command.id.c_str(),
                        command.name.c_str(), command.address.c_str(), command.model ? 1 : 0,
                        command.type);
        for (const auto& report : device.reportTemplates)
            std::printf("  report|%s|%s|%s|type=%d|%s\n", report.id.c_str(), report.name.c_str(),
                        report.address.c_str(), report.type, parameters(report.parameters).c_str());
    }

    // ConfigTemplateServer sends templates in the same shape: `type` as a number, no `valueType`.
    JsonDocument served;
    assert(!deserializeJson(served, riot2::serializeDeviceConfigurations(node.deviceConfigurations)));
    size_t d = 0;
    for (JsonObjectConst device : served.as<JsonArrayConst>()) {
        const auto& parsed = node.deviceConfigurations[d++];
        size_t c = 0, r = 0;
        for (JsonObjectConst command : device["commandTemplates"].as<JsonArrayConst>()) {
            assert(command["type"].is<int>() && command["type"].as<int>() == parsed.commandTemplates[c++].type);
            assert(command["valueType"].isNull());
        }
        for (JsonObjectConst report : device["reportTemplates"].as<JsonArrayConst>())
            assert(report["type"].is<int>() && report["type"].as<int>() == parsed.reportTemplates[r++].type);
        assert(c == parsed.commandTemplates.size() && r == parsed.reportTemplates.size());
    }
    assert(d == node.deviceConfigurations.size());
}
