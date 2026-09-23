#pragma once

#include <Arduino.h>

// Main-loop-only scheduler; a new notification supersedes an older failed request.
class ConfigurationRetry {
public:
    void request(const String& url) {
        _url = url;
        _pending = true;
        _waiting = false;
        _backoffMs = 1000;
    }

    template <typename Fetch>
    void loop(bool connected, Fetch fetch) {
        if (!_pending || !connected) return;
        if (_waiting && static_cast<uint32_t>(millis() - _failedAtMs) < _waitMs) return;
        if (fetch(_url)) {
            _pending = false;
            _waiting = false;
            _backoffMs = 1000;
            return;
        }
        _failedAtMs = millis();
        _waitMs = _backoffMs;
        _backoffMs = _backoffMs < 16000 ? _backoffMs * 2 : 30000;
        _waiting = true;
    }

private:
    String _url;
    bool _pending = false;
    bool _waiting = false;
    uint32_t _failedAtMs = 0;
    uint32_t _waitMs = 0;
    uint32_t _backoffMs = 1000;
};
