#include <riot2/ConfigTemplateServer.h>

#include <riot2/DeviceConfigurationJson.h>

void ConfigTemplateServer::begin(TemplatesProvider provider) {
    _provider = std::move(provider);
    _server.on("/api/device/configuration/templates", HTTP_GET, [this]() { handleTemplates(); });
    _server.begin();
}

void ConfigTemplateServer::loop() {
    _server.handleClient();
}

void ConfigTemplateServer::handleTemplates() {
    std::vector<DeviceConfiguration> templates = _provider ? _provider() : std::vector<DeviceConfiguration>();
    _server.send(200, "application/json", riot2::serializeDeviceConfigurations(templates));
}
