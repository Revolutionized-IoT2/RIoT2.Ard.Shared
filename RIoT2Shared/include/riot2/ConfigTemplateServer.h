#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <functional>
#include <vector>

#include <riot2/DeviceConfiguration.h>

// Serves GET /api/device/configuration/templates (matches
// RIoT2.Core.Constants.ApiConfigurationTemplateUrl), returning this node's
// available device configuration templates - one per registered View/
// IPeripheral classFullName (see riot2::Factory::configurationTemplates()) -
// as JSON, mirroring RIoT2.Net.Node's own endpoint of the same path. Lets an
// orchestrator UI show the end user what a given view/peripheral's
// configuration should look like before they actually configure this node.
//
// Must be reachable as soon as the node has an IP address - the
// orchestrator discovers it via the node's NodeOnlineMessage.nodeBaseUrl
// (see MqttConnection::publishOnline()), which can arrive well before this
// node's own orchestrator handshake/device configuration fetch completes -
// so begin()/loop() should run unconditionally once Wi-Fi setup has
// started, independent of MQTT/provisioning state.
//
// Call begin() once (with a callback that assembles the current list of
// templates), then loop() on every Arduino loop() iteration.
class ConfigTemplateServer {
public:
    using TemplatesProvider = std::function<std::vector<DeviceConfiguration>()>;

    void begin(TemplatesProvider provider);
    void loop();

private:
    WebServer _server{80};
    TemplatesProvider _provider;

    void handleTemplates();
};
