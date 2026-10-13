// Golden-file contract test (I05, plan M7 step 3): the board's real MQTT handlers and the shared
// OrchestratorClient::parseConfiguration read the golden files from RIoT2.Core/contracts/golden.
// test_golden_messages.py injects the production code and the golden JSON, runs this program and
// compares its output with what Python's json module reads from the same files.
#include <cassert>
#include <cstdio>
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

// Board globals the handlers use.
const char* const kOtaCommandId = "system.ota";
struct Buzzer { static void confirm() {} static void error() {} };
struct HapticFeedback {
    static HapticFeedback& instance() { static HapticFeedback feedback; return feedback; }
    void confirm() {}
    void error() {}
};
struct PopupOverlay { void showAlert(const char*, const char*) {} } popupOverlay;
struct OtaUpdater { static bool performUpdate(const String&) { return true; } };
void showStatus(const char*, const char*) {}
void wakeDisplay() {}
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
} viewManager, peripheralManager;
struct Retry { String url; void request(const String& apiBaseUrl) { url = apiBaseUrl; } } configurationRetry;
bool tabsInitialized = true;
struct Splash { void setConfigStatus(const char*) {} } splashView;
// PRODUCTION_HANDLERS
// GOLDEN_DATA

String parameters(const ParameterList& list) {
    String text;
    for (const auto& kv : list) text += (text.empty() ? "" : ",") + kv.first + "=" + kv.second;
    return text;
}

int main() {
    for (const auto& golden : kGoldenCommands) {
        viewManager.calls = peripheralManager.calls = 0;
        handleCommand("riot2/node/golden/command", golden);
        assert(viewManager.calls == 1 && peripheralManager.calls == 1);
        std::printf("command|%s|%s\n", viewManager.id.c_str(), viewManager.value.c_str());
    }

    handleConfigurationMessage("riot2/node/golden/configuration", kGoldenConfigurationCommand);
    std::printf("configuration|%s\n", configurationRetry.url.c_str());

    OrchestratorClient client;
    NodeConfiguration node;
    assert(client.parseConfiguration(kGoldenNodeConfiguration, node));
    std::printf("node|%s|%s\n", node.id.c_str(), node.name.c_str());
    for (const auto& device : node.deviceConfigurations) {
        std::printf("device|%s|%s|%s|%s\n", device.id.c_str(), device.name.c_str(),
                    device.classFullName.c_str(), parameters(device.deviceParameters).c_str());
        for (const auto& command : device.commandTemplates)
            std::printf("  command|%s|%s|%s|model=%d|type=%s|valueType=%d\n", command.id.c_str(),
                        command.name.c_str(), command.address.c_str(), command.model ? 1 : 0,
                        command.type.c_str(), command.valueType);
        for (const auto& report : device.reportTemplates)
            std::printf("  report|%s|%s|%s|%s\n", report.id.c_str(), report.name.c_str(),
                        report.address.c_str(), parameters(report.parameters).c_str());
    }
}
