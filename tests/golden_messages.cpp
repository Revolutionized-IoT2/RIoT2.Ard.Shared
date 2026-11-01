// Golden-file contract test (I05, plan M7 step 3; I19): NodeRuntime's real MQTT handlers (both
// boards use them since I18), OrchestratorClient::parseConfiguration, the command results and the
// configuration status read and write the golden files in RIoT2.Core/contracts/golden.
// test_golden_messages.py injects the production code and the golden JSON, runs this program and
// compares its output with what Python's json module reads from the same files.
// RUNTIME_HARNESS
struct OrchestratorClient {
    bool parseConfiguration(const String& json, NodeConfiguration& out);
};
// PRODUCTION_PARSE_CONFIGURATION
namespace riot2 {
String serializeDeviceConfigurations(const std::vector<DeviceConfiguration>& configs);
}
// PRODUCTION_SERIALIZE_CONFIGURATIONS
// GOLDEN_DATA

String parameters(const ParameterList& list) {
    String text;
    for (const auto& kv : list) text = text + (text.empty() ? "" : ",") + kv.first + "=" + kv.second;
    return text;
}

struct Views {
    unsigned calls = 0;
    String id, value;
    bool handles = false;
    bool onCommand(const Command& command) {
        ++calls;
        id = command.id;
        std::string text;
        serializeJson(command.value, text);
        value = String(text);
        return handles;
    }
};

int main() {
    riot2::NodeRuntime runtime;
    Views views;
    unsigned requested = 0;
    runtime._hooks.onCommand = [&](const Command& command) { return views.onCommand(command); };
    runtime._hooks.onConfigurationRequested = [&] { ++requested; };
    for (const auto& golden : kGoldenCommands) {
        views.calls = runtime._peripherals.calls = 0;
        runtime.handleCommand("riot2/node/golden/command", golden);
        assert(views.calls == 1 && runtime._peripherals.calls == 1);
        std::printf("command|%s|%s\n", views.id.c_str(), views.value.c_str());
    }
    // Commands without a correlationId (an orchestrator before 1.4.0) get no result.
    assert(runtime._mqtt.published.empty());

    runtime.handleConfigurationMessage("riot2/node/golden/configuration", kGoldenConfigurationCommand);
    assert(requested == 1);
    std::printf("configuration|%s\n", runtime._pendingHash.empty() ? "" : "unexpected hash");
    runtime.handleConfigurationMessage("riot2/node/golden/configuration", kGoldenConfigurationCommandHash);
    assert(requested == 2 && runtime._configurationRetry.pending());
    std::printf("configuration-hash|%s\n", runtime._pendingHash.c_str());

    OrchestratorClient client;
    NodeConfiguration node;
    assert(client.parseConfiguration(kGoldenNodeConfiguration, node));
    std::printf("node|%s|%s|%ld|%s\n", node.id.c_str(), node.name.c_str(), node.revision, node.hash.c_str());
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

    // Design 7.2: the hashed configuration (orchestrator 1.5.0 and later) is fetched, applied and
    // reported in the status.
    NodeConfiguration hashed;
    assert(client.parseConfiguration(kGoldenNodeConfigurationHash, hashed));
    std::printf("node-hash|%ld|%s\n", hashed.revision, hashed.hash.c_str());
    runtime._orchestrator.configuration = hashed;
    runtime._failedDevices = {{"golden-device", "Port COM3 not found"}};
    runtime.runFetch([](const String&) { return true; });
    std::printf("status|%s\n", runtime._mqtt.published.back().payload.c_str());
    assert(runtime._mqtt.published.back().topic == "riot2/node/golden/status" && runtime._mqtt.published.back().retained);
    runtime._configurationRetry.request("http://192.0.2.1:5000/");
    runtime._orchestrator.error = "GET http://192.0.2.1:5000/api/nodes/golden/configuration failed: 503";
    runtime.runFetch([](const String&) { return false; });
    std::printf("status-failed|%s\n", runtime._mqtt.published.back().payload.c_str());
    // While a fetch is pending an unknown command is `rejected`; the checks below want none pending.
    runtime._configurationRetry.cancel();

    // Design 7.1: a correlated command gets a result.
    views.handles = true;
    runtime.handleCommand("riot2/node/golden/command", kGoldenCorrelatedCommand);
    assert(runtime._mqtt.published.back().topic == "riot2/node/golden/command/result" &&
           !runtime._mqtt.published.back().retained);
    std::printf("result|%s\n", runtime._mqtt.published.back().payload.c_str());
    views.handles = false;
    runtime.handleCommand("riot2/node/golden/command",
                          R"({"id":"golden-command-number","value":1,"correlationId":"golden-correlation-2"})");
    std::printf("result-unknown|%s\n", runtime._mqtt.published.back().payload.c_str());
}
