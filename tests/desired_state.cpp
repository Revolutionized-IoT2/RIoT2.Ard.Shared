// Firmware command results and desired state (I19; designs 7.1 phase 3 and 7.2 phase 5), run on
// NodeRuntime's real handlers, CorrelationRing and NodeStatus (tests/runtime_harness.h).
// RUNTIME_HARNESS

const char* const kTopic = "riot2/node/golden/command";
const char* const kHash1 = "1111111111111111111111111111111111111111111111111111111111111111";
const char* const kHash2 = "2222222222222222222222222222222222222222222222222222222222222222";

JsonDocument parse(const String& payload) {
    JsonDocument doc;
    assert(!deserializeJson(doc, payload));
    return doc;
}

const Published& last(riot2::NodeRuntime& runtime) {
    assert(!runtime._mqtt.published.empty());
    return runtime._mqtt.published.back();
}

String configurationCommand(const char* hash) {
    String command = R"({"apiBaseUrl":"http://192.0.2.1:5000/")";
    if (hash) command = command + R"(,"revision":7,"hash":")" + hash + "\"";
    return command + "}";
}

String command(const char* id, const char* correlationId) {
    String text = String(R"({"id":")") + id + R"(","value":true)";
    if (correlationId) text = text + R"(,"correlationId":")" + correlationId + "\"";
    return text + "}";
}

NodeConfiguration configuration(long revision, const char* hash) {
    NodeConfiguration node;
    node.id = "golden";
    node.revision = revision;
    node.hash = hash;
    DeviceConfiguration view;
    view.id = "view-1";
    view.classFullName = "RIoT2.Test.View";
    view.commandTemplates.push_back({"view-command"});
    DeviceConfiguration peripheral;
    peripheral.id = "peripheral-1";
    peripheral.classFullName = "RIoT2.Test.Peripheral";
    peripheral.commandTemplates.push_back({"peripheral-command"});
    DeviceConfiguration unknown;
    unknown.id = "unknown-1";
    unknown.classFullName = "RIoT2.Other.Board.View";
    node.deviceConfigurations = {view, peripheral, unknown};
    return node;
}

struct Node {
    riot2::NodeRuntime runtime;
    unsigned viewCalls = 0;
    unsigned fetches = 0;
    unsigned requested = 0;
    unsigned rebuilds = 0;
    Node() {
        runtime._hooks.isView = [](const String& name) { return name == "RIoT2.Test.View"; };
        runtime._hooks.onCommand = [this](const Command& command) {
            ++viewCalls;
            return command.id == "view-command";
        };
        runtime._hooks.onConfiguration = [this](const NodeConfiguration&) {
            ++rebuilds;
            return false;
        };
        runtime._hooks.onConfigurationRequested = [this] { ++requested; };
        runtime._peripherals.owned = {"peripheral-command"};
    }
    // NodeRuntime::loop()'s fetch, with OrchestratorClient's outcome: a changed configuration is
    // handed to handleConfiguration(), an unchanged one (or a 304) is not.
    void fetch(bool ok, const NodeConfiguration* changed = nullptr, const char* error = "") {
        runtime.runFetch([&](const String&) {
            ++fetches;
            runtime._orchestrator.error = ok ? "" : error;
            if (ok && changed) {
                runtime._orchestrator.configuration = *changed;
                runtime.handleConfiguration(*changed);
            }
            return ok;
        });
    }
};

void statusBeforeTheFirstConfiguration() {
    Node node;
    node.runtime.publishStatus();
    const Published& status = last(node.runtime);
    assert(status.topic == "riot2/node/golden/status" && status.retained);
    JsonDocument doc = parse(status.payload);
    assert(doc["state"] == "applying");
    assert(doc["appliedRevision"].isNull() && doc["appliedHash"].isNull() && doc["error"].isNull());
    assert(doc["timeStamp"].is<uint32_t>());
}

// Compatibility matrix, new orchestrator and new node: the applied hash is never fetched again.
void newOrchestratorSkipsTheAppliedHash() {
    Node node;
    NodeConfiguration first = configuration(7, kHash1);
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash1));
    assert(node.requested == 1 && node.runtime._configurationRetry.pending());
    node.fetch(true, &first);
    assert(node.fetches == 1 && node.rebuilds == 1 && !node.runtime._configurationRetry.pending());
    JsonDocument status = parse(last(node.runtime).payload);
    assert(status["state"] == "applied" && status["appliedRevision"] == 7 && status["appliedHash"] == kHash1);
    assert(status["failedDevices"].size() == 1 && status["failedDevices"][0]["id"] == "unknown-1");
    assert(status["failedDevices"][0]["message"] == "Unknown class");

    // An MQTT reconnect or an orchestrator restart: one status message, no fetch, no rebuild.
    size_t published = node.runtime._mqtt.published.size();
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash1));
    node.fetch(true);
    assert(node.requested == 1 && node.fetches == 1 && node.rebuilds == 1);
    assert(node.runtime._mqtt.published.size() == published + 1 && last(node.runtime).retained);
    assert(parse(last(node.runtime).payload)["appliedHash"] == kHash1);
}

// A failed fetch keeps the applied configuration and reports `failed`; a repeat doesn't reset the
// backoff, and the applied hash coming back ends the failed state with a fetch (a 304).
void failedFetchKeepsTheConfiguration() {
    Node node;
    NodeConfiguration first = configuration(7, kHash1);
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash1));
    node.fetch(true, &first);

    node.runtime.handleConfigurationMessage("", configurationCommand(kHash2));
    node.fetch(false, nullptr, "GET http://192.0.2.1:5000/api/nodes/golden/configuration failed: 503");
    JsonDocument status = parse(last(node.runtime).payload);
    assert(status["state"] == "failed" && status["appliedHash"] == kHash1 && status["appliedRevision"] == 7);
    assert(status["error"] == "GET http://192.0.2.1:5000/api/nodes/golden/configuration failed: 503");
    size_t published = node.runtime._mqtt.published.size();

    // The same hash again while its fetch waits for the backoff: no new request, no fetch now.
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash2));
    node.fetch(false, nullptr, "GET http://192.0.2.1:5000/api/nodes/golden/configuration failed: 503");
    assert(node.requested == 2 && node.fetches == 2);
    hostMillis += 1000;
    node.fetch(false, nullptr, "GET http://192.0.2.1:5000/api/nodes/golden/configuration failed: 503");
    assert(node.fetches == 3);
    // The same failure again isn't published again.
    assert(node.runtime._mqtt.published.size() == published);

    // The orchestrator wants hash 1 again: the node isn't `applied`, so it fetches (and gets a 304).
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash1));
    assert(node.requested == 3);
    node.fetch(true);
    assert(node.fetches == 4 && node.rebuilds == 1);
    status = parse(last(node.runtime).payload);
    assert(status["state"] == "applied" && status["appliedHash"] == kHash1 && status["error"].isNull());

    // A pending fetch for another hash is dropped when the applied one is wanted.
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash2));
    assert(node.runtime._configurationRetry.pending());
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash1));
    assert(!node.runtime._configurationRetry.pending());
}

// Compatibility matrix, old orchestrator (no hash) and new node: every command is fetched, and the
// status has no revision or hash.
void oldOrchestratorAlwaysFetches() {
    Node node;
    NodeConfiguration legacy = configuration(-1, "");
    node.runtime.handleConfigurationMessage("", configurationCommand(nullptr));
    node.fetch(true, &legacy);
    node.runtime.handleConfigurationMessage("", configurationCommand(nullptr));
    node.fetch(true);
    assert(node.requested == 2 && node.fetches == 2 && node.rebuilds == 1);
    JsonDocument status = parse(last(node.runtime).payload);
    assert(status["state"] == "applied" && status["appliedRevision"].isNull() && status["appliedHash"].isNull());
}

// Compatibility matrix, old orchestrator: a command without a correlationId runs and gets no result.
void commandWithoutCorrelationIdGetsNoResult() {
    Node node;
    node.runtime.handleCommand(kTopic, command("view-command", nullptr));
    node.runtime.handleCommand(kTopic, command("view-command", nullptr));
    assert(node.viewCalls == 2 && node.runtime._mqtt.published.empty());
}

JsonDocument lastResult(Node& node) {
    const Published& result = last(node.runtime);
    assert(result.topic == "riot2/node/golden/command/result" && !result.retained);
    return parse(result.payload);
}

void correlatedCommands() {
    Node node;
    // Before the first configuration an unknown command is rejected and forgotten.
    node.runtime.handleCommand(kTopic, command("view-command-2", "c-early"));
    JsonDocument result = lastResult(node);
    assert(result["status"] == "rejected" && result["correlationId"] == "c-early");
    assert(result["commandId"] == "view-command-2" && result["timeStamp"].is<uint32_t>());
    node.runtime.handleCommand(kTopic, command("view-command-2", "c-early"));
    assert(node.viewCalls == 2 && lastResult(node)["status"] == "rejected");

    NodeConfiguration first = configuration(7, kHash1);
    node.runtime.handleConfigurationMessage("", configurationCommand(kHash1));
    // ...and while the configuration is fetched.
    node.runtime.handleCommand(kTopic, command("view-command-2", "c-pending"));
    assert(lastResult(node)["status"] == "rejected");
    node.fetch(true, &first);

    node.runtime.handleCommand(kTopic, command("view-command", "c-1"));
    result = lastResult(node);
    assert(result["status"] == "executed" && result["error"].isNull() && node.viewCalls == 4);
    // A redelivery is answered `duplicate` and not run again.
    node.runtime.handleCommand(kTopic, command("view-command", "c-1"));
    assert(lastResult(node)["status"] == "duplicate" && node.viewCalls == 4);

    node.runtime.handleCommand(kTopic, command("peripheral-command", "c-2"));
    assert(lastResult(node)["status"] == "executed" && node.runtime._peripherals.lastId == "peripheral-command");

    node.runtime.handleCommand(kTopic, command("missing", "c-3"));
    result = lastResult(node);
    assert(result["status"] == "unknownCommand");
    String error = result["error"].as<const char*>();
    assert(error == "No view or peripheral on this node has command missing.");
    // A failed command is answered with its original outcome.
    node.runtime.handleCommand(kTopic, command("missing", "c-3"));
    result = lastResult(node);
    assert(result["status"] == "unknownCommand" && String(result["error"].as<const char*>()) == error);
    unsigned calls = node.viewCalls;

    // The ring keeps the last 16 ids: c-1 drops out after 16 more and runs again.
    for (int i = 0; i < 16; ++i) {
        node.runtime.handleCommand(kTopic, command("view-command", ("fill-" + String(i)).c_str()));
    }
    node.runtime.handleCommand(kTopic, command("view-command", "c-1"));
    assert(lastResult(node)["status"] == "executed" && node.viewCalls == calls + 17);
}

void otaCommands() {
    Node node;
    unsigned failures = 0;
    node.runtime._hooks.onOtaFailed = [&] { ++failures; };
    const char* ota = R"({"id":"system.ota","value":"http://192.0.2.1/firmware.bin","correlationId":"ota-1"})";
    OtaUpdater::succeeds = false;
    node.runtime.handleCommand(kTopic, ota);
    JsonDocument result = lastResult(node);
    assert(failures == 1 && result["status"] == "failed" && result["commandId"] == "system.ota");
    assert(riot2::savedOtaCommands.empty());
    // A failed update is final: the redelivery isn't downloaded again.
    node.runtime.handleCommand(kTopic, ota);
    assert(OtaUpdater::urls.size() == 1 && lastResult(node)["status"] == "failed");

    // An installed image reports `executed` before the restart and keeps the id across it.
    OtaUpdater::succeeds = true;
    node.runtime.handleCommand(kTopic, R"({"id":"system.ota","value":"http://192.0.2.1/firmware.bin","correlationId":"ota-2"})");
    assert(lastResult(node)["status"] == "executed" && failures == 1);
    assert(riot2::savedOtaCommands.size() == 1 && riot2::savedOtaCommands[0] == "ota-2");
    // Without a correlationId nothing is saved or published.
    size_t published = node.runtime._mqtt.published.size();
    node.runtime.handleCommand(kTopic, R"({"id":"system.ota","value":"http://192.0.2.1/firmware.bin"})");
    assert(node.runtime._mqtt.published.size() == published && riot2::savedOtaCommands.size() == 1);
}

void statusFitsOnePacket() {
    std::vector<riot2::FailedDevice> failed;
    for (int i = 0; i < 6; ++i) {
        failed.push_back({"00000000-0000-0000-0000-00000000000" + String(i), "Unknown class"});
    }
    riot2::NodeStatus status;
    status.applied(123456, kHash1, failed);
    JsonDocument doc;
    status.write(doc, 1790000000u, 512);
    assert(doc["failedDevices"].size() == riot2::NodeStatus::kMaxFailedDevices);
    std::string payload;
    serializeJson(doc, payload);
    // A packet too small for four drops more, down to none.
    status.write(doc, 1790000000u, payload.size() - 1);
    assert(doc["failedDevices"].size() == 3);
    status.write(doc, 1790000000u, 10);
    assert(doc["failedDevices"].isNull() && doc["appliedHash"] == kHash1);

    // Long errors are cut; the same error twice is one change.
    assert(status.failed(String(std::string(300, 'e'))));
    status.write(doc, 1790000000u, 512);
    assert(String(doc["error"].as<const char*>()).length() == riot2::NodeStatus::kMaxErrorLength);
    assert(!status.failed(String(std::string(300, 'e'))));
    assert(status.failed(""));
    status.write(doc, 1790000000u, 512);
    assert(doc["error"] == "The configuration could not be applied.");
    assert(!status.isApplied(kHash1));
    status.applied(1, kHash1, {});
    assert(status.isApplied(kHash1) && !status.isApplied("") && !status.isApplied(kHash2));
}

int main() {
    statusBeforeTheFirstConfiguration();
    newOrchestratorSkipsTheAppliedHash();
    failedFetchKeepsTheConfiguration();
    oldOrchestratorAlwaysFetches();
    commandWithoutCorrelationIdGetsNoResult();
    correlatedCommands();
    otaCommands();
    statusFitsOnePacket();
    std::printf("ok\n");
}
