#include <riot2/ProvisioningPortal.h>

#include <WiFi.h>

namespace {

String htmlEscape(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            default:
                out += c;
        }
    }
    return out;
}

}  // namespace

// Out-of-line definitions required pre-C++17 whenever a static constexpr
// member is ODR-used (e.g. its address taken, or passed by reference).
constexpr uint16_t ProvisioningPortal::kDnsPort;
constexpr unsigned long ProvisioningPortal::kRestartDelayMs;

void ProvisioningPortal::begin() {
    uint64_t mac = ESP.getEfuseMac();
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%04X", static_cast<uint16_t>(mac & 0xFFFF));
    _apSsid = String("RIoT2-Setup-") + suffix;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apSsid.c_str());
    _apIp = WiFi.softAPIP();

    _dnsServer.start(kDnsPort, "*", _apIp);

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();

    Serial.printf("[Provisioning] AP \"%s\" started, browse to http://%s/\n", _apSsid.c_str(),
                  _apIp.toString().c_str());
}

void ProvisioningPortal::loop() {
    _dnsServer.processNextRequest();
    _server.handleClient();

    if (_restartRequestedAtMs != 0 && (millis() - _restartRequestedAtMs) >= kRestartDelayMs) {
        ESP.restart();
    }
}

String ProvisioningPortal::renderForm(const NodeConfig& values, const String& errorMessage) {
    String html;
    html.reserve(2048);
    html += "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>RIoT2 Node Setup</title></head><body>";
    html += "<h2>RIoT2 Node Setup</h2>";
    if (errorMessage.length()) {
        html += "<p style=\"color:red\">" + htmlEscape(errorMessage) + "</p>";
    }
    html += "<form method=\"POST\" action=\"/save\">";
    html += "<label>Node Id</label><br><input name=\"id\" value=\"" + htmlEscape(values.id) +
            "\" style=\"width:100%\"><br><br>";
    html += "<label>Node Name</label><br><input name=\"name\" value=\"" + htmlEscape(values.name) +
            "\" style=\"width:100%\"><br><br>";
    html += "<label>Wi-Fi SSID</label><br><input name=\"ssid\" value=\"" + htmlEscape(values.wifiSsid) +
            "\" style=\"width:100%\"><br><br>";
    html += "<label>Wi-Fi Password</label><br><input type=\"password\" name=\"pass\" value=\"" +
            htmlEscape(values.wifiPassword) + "\" style=\"width:100%\"><br><br>";
    html += "<label>MQTT Server (host[:port])</label><br><input name=\"mqttUrl\" value=\"" +
            htmlEscape(values.mqttServerUrl) + "\" style=\"width:100%\"><br><br>";
    html += "<label>MQTT Username</label><br><input name=\"mqttUser\" value=\"" +
            htmlEscape(values.mqttUsername) + "\" style=\"width:100%\"><br><br>";
    html += "<label>MQTT Password</label><br><input type=\"password\" name=\"mqttPass\" value=\"" +
            htmlEscape(values.mqttPassword) + "\" style=\"width:100%\"><br><br>";
    html += "<label><input type=\"checkbox\" name=\"mqttTls\" value=\"1\"" +
            String(values.mqttUseTls ? " checked" : "") + "> Use TLS for MQTT</label><br><br>";
    // Checked by default (mirrors NodeConfig::vibrateEnabled's true default)
    // so the very first provisioning save - where `values` is still an
    // in-memory default-constructed NodeConfig, not yet loaded from NVS -
    // doesn't silently disable it just because the user never touched the
    // checkbox (an unchecked HTML checkbox is omitted from POST entirely).
    html += "<label><input type=\"checkbox\" name=\"vibrate\" value=\"1\"" +
            String(values.vibrateEnabled ? " checked" : "") + "> Enable vibration feedback</label><br><br>";
    html += "<button type=\"submit\">Save &amp; Restart</button>";
    html += "</form></body></html>";
    return html;
}

void ProvisioningPortal::handleRoot() {
    NodeConfig current = NodeConfigStore::load();
    if (current.id.length() == 0) {
        current.id = NodeConfigStore::generateDefaultId();
    }
    _server.send(200, "text/html", renderForm(current, ""));
}

void ProvisioningPortal::handleSave() {
    NodeConfig config;
    config.id = _server.arg("id");
    config.name = _server.arg("name");
    config.wifiSsid = _server.arg("ssid");
    config.wifiPassword = _server.arg("pass");
    config.mqttServerUrl = _server.arg("mqttUrl");
    config.mqttUsername = _server.arg("mqttUser");
    config.mqttPassword = _server.arg("mqttPass");
    config.mqttUseTls = _server.hasArg("mqttTls");
    config.vibrateEnabled = _server.hasArg("vibrate");

    if (!config.isValid()) {
        _server.send(200, "text/html",
                      renderForm(config, "Node Id, Wi-Fi SSID and MQTT Server are required."));
        return;
    }

    NodeConfigStore::save(config);

    String html =
        "<!DOCTYPE html><html><body><h2>Saved</h2>"
        "<p>Configuration saved. The device is restarting...</p></body></html>";
    _server.send(200, "text/html", html);

    Serial.println("[Provisioning] Config saved, restarting shortly...");
    _restartRequestedAtMs = millis();
}

void ProvisioningPortal::handleNotFound() {
    // Captive portal probe URLs (Android/iOS/Windows) -> redirect to the setup form
    // so the OS shows the "sign in to network" prompt automatically.
    _server.sendHeader("Location", String("http://") + _apIp.toString() + "/", true);
    _server.send(302, "text/plain", "");
}
