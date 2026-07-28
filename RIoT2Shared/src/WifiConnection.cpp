#include <riot2/WifiConnection.h>

#include <WiFi.h>

void WifiConnection::begin(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
    _state = WifiState::Disconnected;
    _backoffMs = kInitialBackoffMs;
    _lastAttemptMs = 0;  // forces an immediate connect attempt on first loop()
}

void WifiConnection::startConnection() {
    Serial.printf("[WiFi] Connecting to \"%s\"...\n", _ssid.c_str());
    WiFi.mode(WIFI_STA);
    // ESP32's default WiFi modem-sleep power saving periodically pauses the
    // radio and can introduce brief scheduling/interrupt latency spikes -
    // enough, on some boards, to noticeably affect input responsiveness.
    // Disable it: these nodes are USB/battery-powered hardware where
    // responsiveness matters far more than the small extra power draw.
    WiFi.setSleep(false);
    WiFi.begin(_ssid.c_str(), _password.c_str());
    _state = WifiState::Connecting;
    _lastAttemptMs = millis();
}

void WifiConnection::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (_state != WifiState::Connected) {
            Serial.printf("[WiFi] Connected, IP=%s\n", WiFi.localIP().toString().c_str());
            _state = WifiState::Connected;
            _backoffMs = kInitialBackoffMs;
        }
        return;
    }

    if (_state == WifiState::Connected) {
        Serial.println("[WiFi] Connection lost");
        _state = WifiState::Disconnected;
        _lastAttemptMs = millis();
        return;
    }

    unsigned long now = millis();

    if (_state == WifiState::Connecting) {
        if ((now - _lastAttemptMs) < kConnectTimeoutMs) {
            return;  // still trying, give it time
        }
        Serial.println("[WiFi] Connect attempt timed out");
        _backoffMs = min(_backoffMs * 2, kMaxBackoffMs);
        _state = WifiState::Disconnected;
        _lastAttemptMs = now;
        return;
    }

    // Disconnected: wait out the backoff, then retry.
    if (_lastAttemptMs == 0 || (now - _lastAttemptMs) >= _backoffMs) {
        startConnection();
    }
}

bool WifiConnection::isConnected() const {
    return _state == WifiState::Connected;
}
