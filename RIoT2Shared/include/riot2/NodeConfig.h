#pragma once

#include <Arduino.h>

// Node provisioning parameters, persisted in NVS via Preferences (namespace
// "riot2node").
//
// Optional DEFAULT_* build flags (see platformio.ini) seed these values into
// NVS the first time the device boots with no stored config, letting
// connectivity be exercised before a provisioning UX is wired up.
struct NodeConfig {
    String id;
    String name;
    String wifiSsid;
    String wifiPassword;
    String mqttServerUrl;
    String mqttUsername;
    String mqttPassword;

    // Opt-in: when true, MqttConnection connects via WiFiClientSecure (TLS)
    // instead of a plaintext WiFiClient, defaulting to port 8883 instead of
    // 1883 if mqttServerUrl doesn't specify one. See riot2/TlsRootCa.h for
    // the certificate used to validate the broker.
    bool mqttUseTls = false;

    // Gates IFeedback::vibrate() on nodes with a vibration motor (e.g.
    // RIoT2.Ard.M5Core2.Node's HapticFeedback) - defaults on since a buzzing
    // node is the more discoverable first-boot experience, and off has no
    // effect at all on boards with no motor (M5Dial.Node just carries the
    // unused field/checkbox).
    bool vibrateEnabled = true;

    bool isValid() const {
        return id.length() > 0 && wifiSsid.length() > 0 && mqttServerUrl.length() > 0;
    }
};

class NodeConfigStore {
public:
    static NodeConfig load();
    static void save(const NodeConfig& config);
    static void clear();

    static String generateDefaultId();
};
