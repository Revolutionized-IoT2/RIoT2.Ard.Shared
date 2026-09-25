#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <IPAddress.h>
#include <WebServer.h>

#include <riot2/NodeConfig.h>

// First-boot provisioning UX.
//
// Starts a Wi-Fi access point plus a captive-portal web form so the
// NodeConfig parameters (Id, Name, WifiSsid, WifiPassword, MqttServerUrl,
// MqttUsername, MqttPassword, MqttUseTls) can be set from a phone/laptop
// without reflashing. Once a valid config is submitted it is persisted via
// NodeConfigStore and the device restarts into normal operation.
//
// Call begin() once, then loop() on every Arduino loop() iteration.
class ProvisioningPortal {
public:
    void begin();
    void loop();

    const String& apSsid() const { return _apSsid; }
    IPAddress apIp() const { return _apIp; }

private:
    static constexpr uint16_t kDnsPort = 53;
    static constexpr unsigned long kRestartDelayMs = 2000;

    DNSServer _dnsServer;
    WebServer _server{80};
    String _apSsid;
    IPAddress _apIp;
    unsigned long _restartRequestedAtMs = 0;

    void handleRoot();
    void handleSave();
    void handleNotFound();
    String renderForm(const NodeConfig& values, const String& errorMessage);
};
