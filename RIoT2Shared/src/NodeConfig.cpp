#include <riot2/NodeConfig.h>

#include <Preferences.h>
#include <WiFi.h>

namespace {
constexpr const char* kNamespace = "riot2node";

// Every RIoT2 Ard node project defines its own fallback via
// -D'RIOT2_DEFAULT_NODE_NAME="..."' in platformio.ini, so the displayed/
// reported name is board-specific (e.g. "M5Dial Node") even before a name
// has ever been explicitly configured, rather than a generic shared-lib
// string.
#ifndef RIOT2_DEFAULT_NODE_NAME
#define RIOT2_DEFAULT_NODE_NAME "RIoT2 Node"
#endif
}  // namespace

NodeConfig NodeConfigStore::load() {
    Preferences prefs;
    prefs.begin(kNamespace, true);

    NodeConfig config;
    config.id = prefs.getString("id", "");
    config.name = prefs.getString("name", "");
    config.wifiSsid = prefs.getString("ssid", "");
    config.wifiPassword = prefs.getString("pass", "");
    config.mqttServerUrl = prefs.getString("mqttUrl", "");
    config.mqttUsername = prefs.getString("mqttUser", "");
    config.mqttPassword = prefs.getString("mqttPass", "");
    config.mqttUseTls = prefs.getBool("mqttTls", false);
    config.vibrateEnabled = prefs.getBool("vibrate", true);
    prefs.end();

#if defined(DEFAULT_NODE_ID)
    if (config.id.length() == 0) {
        config.id = DEFAULT_NODE_ID;
#if defined(DEFAULT_NODE_NAME)
        config.name = DEFAULT_NODE_NAME;
#endif
#if defined(DEFAULT_WIFI_SSID)
        config.wifiSsid = DEFAULT_WIFI_SSID;
#endif
#if defined(DEFAULT_WIFI_PASSWORD)
        config.wifiPassword = DEFAULT_WIFI_PASSWORD;
#endif
#if defined(DEFAULT_MQTT_SERVER_URL)
        config.mqttServerUrl = DEFAULT_MQTT_SERVER_URL;
#endif
#if defined(DEFAULT_MQTT_USERNAME)
        config.mqttUsername = DEFAULT_MQTT_USERNAME;
#endif
#if defined(DEFAULT_MQTT_PASSWORD)
        config.mqttPassword = DEFAULT_MQTT_PASSWORD;
#endif
        save(config);
    }
#endif

    if (config.name.length() == 0) {
        config.name = RIOT2_DEFAULT_NODE_NAME;
    }

    return config;
}

void NodeConfigStore::save(const NodeConfig& config) {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putString("id", config.id);
    prefs.putString("name", config.name);
    prefs.putString("ssid", config.wifiSsid);
    prefs.putString("pass", config.wifiPassword);
    prefs.putString("mqttUrl", config.mqttServerUrl);
    prefs.putString("mqttUser", config.mqttUsername);
    prefs.putString("mqttPass", config.mqttPassword);
    prefs.putBool("mqttTls", config.mqttUseTls);
    prefs.putBool("vibrate", config.vibrateEnabled);
    prefs.end();
}

void NodeConfigStore::clear() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.clear();
    prefs.end();
}

String NodeConfigStore::generateDefaultId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[13];
    snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
