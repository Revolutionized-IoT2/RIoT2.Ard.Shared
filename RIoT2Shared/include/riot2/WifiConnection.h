#pragma once

#include <Arduino.h>

enum class WifiState {
    Disconnected,
    Connecting,
    Connected,
};

// Non-blocking Wi-Fi connection manager with retry/backoff.
//
// Call begin() once with credentials, then call loop() on every
// Arduino loop() iteration; it never blocks.
class WifiConnection {
public:
    void begin(const String& ssid, const String& password);
    void loop();

    bool isConnected() const;
    WifiState state() const { return _state; }

private:
    static constexpr unsigned long kInitialBackoffMs = 1000;
    static constexpr unsigned long kMaxBackoffMs = 30000;
    static constexpr unsigned long kConnectTimeoutMs = 15000;

    String _ssid;
    String _password;
    WifiState _state = WifiState::Disconnected;
    unsigned long _lastAttemptMs = 0;
    unsigned long _backoffMs = kInitialBackoffMs;

    void startConnection();
};
