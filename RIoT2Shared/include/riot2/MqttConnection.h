#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <functional>

#include <riot2/NodeConfig.h>
#include <riot2/Report.h>

enum class MqttState {
    Disconnected,
    Connecting,
    Connected,
};

// Non-blocking MQTT connection manager.
//
// Handles the RIoT2 node lifecycle: connects with a Last Will and Testament
// of {"isOnline": false} on riot2/node/{id}/online, retries with backoff,
// publishes the NodeOnlineMessage on (re)connect, and subscribes to
// riot2/orchestrator/online + riot2/node/{id}/configuration +
// riot2/node/{id}/command.
// Invalid broker URLs (empty host, empty/non-numeric/out-of-range explicit
// port) are logged and leave MQTT disconnected.
//
// When NodeConfig::mqttUseTls is set, connects via WiFiClientSecure
// (defaulting to port 8883 instead of 1883 if mqttServerUrl doesn't specify
// one), validated against riot2::rootCaPem() - see TlsRootCa.h.
//
// Call begin() once, then loop() on every Arduino loop() iteration.
class MqttConnection {
public:
    using MessageCallback = std::function<void(const String& topic, const String& payload)>;

    void begin(const NodeConfig& config);
    void loop();

    bool isConnected();
    MqttState state() const { return _state; }

    // Publishes {"isOnline": false} and disconnects. Call on a controlled
    // shutdown/reset path, in addition to the LWT covering dirty disconnects.
    void publishOfflineAndDisconnect();

    // Publishes a view-generated Report to riot2/node/{id}/report as
    // {"id": report.id, "timeStamp": <unix epoch seconds>, "value": <raw>}.
    // No-op if not currently connected. `report.value` is embedded as a raw
    // JSON value (not re-quoted); see Report.h for the invariant.
    void publishReport(const Report& report);

    // Publishes the retained NodeOnlineMessage ({"name","isOnline":true,
    // "nodeType"}) to riot2/node/{id}/online. Called automatically on every
    // (re)connect, and also exposed publicly so the app can re-announce in
    // response to an inbound riot2/orchestrator/online message, per the
    // corrected handshake: orchestrator/online (empty) -> node re-publishes
    // its online message -> orchestrator replies on riot2/node/{id}/configuration
    // with {"apiBaseUrl": ...}.
    void publishOnline();

    void onCommand(MessageCallback callback) { _commandCallback = callback; }
    void onOrchestratorOnline(MessageCallback callback) { _orchestratorCallback = callback; }
    void onConfigurationMessage(MessageCallback callback) { _configurationCallback = callback; }

    // Nested verbatim into the "manifest" field of the NodeOnlineMessage
    // published by publishOnline(). Each project generates its own
    // Manifest::json at build time (see generate_manifest.py) and passes it
    // in here - MqttConnection itself stays project-agnostic. Must be called
    // before the first publishOnline() (i.e. before begin()); the pointer is
    // stored as-is, so it must outlive this object (Manifest::json is a
    // static string constant, so this is always safe in practice).
    void setManifestJson(const char* manifestJson) { _manifestJson = manifestJson; }

private:
    static constexpr unsigned long kInitialBackoffMs = 1000;
    static constexpr unsigned long kMaxBackoffMs = 30000;

    WiFiClient _plainClient;
    WiFiClientSecure _secureClient;
    PubSubClient _client{_plainClient};
    NodeConfig _config;
    // Backing storage for the broker host, kept alive for the lifetime of
    // this object: PubSubClient::setServer(const char*, ...) stores the raw
    // pointer it's given rather than copying it, so it must point at memory
    // that outlives begin().
    String _brokerHost;
    uint16_t _brokerPort = 1883;
    bool _serverConfigured = false;
    MqttState _state = MqttState::Disconnected;
    unsigned long _lastAttemptMs = 0;
    unsigned long _backoffMs = kInitialBackoffMs;

    MessageCallback _commandCallback;
    MessageCallback _orchestratorCallback;
    MessageCallback _configurationCallback;
    const char* _manifestJson = nullptr;

    void attemptConnect();
    void handleMessage(char* topic, byte* payload, unsigned int length);

    static void staticCallback(char* topic, byte* payload, unsigned int length);
    static MqttConnection* _instance;
};
