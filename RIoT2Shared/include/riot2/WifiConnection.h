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

    // Controls whether startConnection() below leaves ESP32's WiFi
    // modem-sleep power saving enabled (true) or disables it (false,
    // default - matches this class's original responsiveness-over-power
    // tradeoff for nodes that never use Bluetooth).
    //
    // MUST be set true before/while any Bluetooth/BLE radio (e.g.
    // NimBLE-Arduino, see BleScanner) is active on the same node: ESP-IDF's
    // WiFi/BT coexistence layer hard-aborts the whole firmware
    // ("E wifi: Error! Should enable WiFi modem sleep when both WiFi and
    // Bluetooth are enabled!!!!!!" followed by abort()/reboot) if WiFi modem
    // sleep is disabled while BT is active - this isn't a recoverable error,
    // it's a fatal assert, and it reproduces as an immediate boot-reboot
    // loop the instant WiFi.begin() runs. Safe to call at any time, even
    // after begin(): takes effect on the next startConnection() call
    // (including the current in-progress one, on the next reconnect retry).
    void setModemSleepEnabled(bool enabled) { _modemSleepEnabled = enabled; }

private:
    static constexpr unsigned long kInitialBackoffMs = 1000;
    static constexpr unsigned long kMaxBackoffMs = 30000;
    static constexpr unsigned long kConnectTimeoutMs = 15000;

    String _ssid;
    String _password;
    WifiState _state = WifiState::Disconnected;
    unsigned long _lastAttemptMs = 0;
    unsigned long _backoffMs = kInitialBackoffMs;
    bool _modemSleepEnabled = false;

    void startConnection();
};
