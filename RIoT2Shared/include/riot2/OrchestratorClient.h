#pragma once

#include <Arduino.h>

#include <functional>

#include <riot2/DeviceConfiguration.h>

// Phase 3 orchestrator handshake.
//
// Given an apiBaseUrl (received on riot2/node/{id}/configuration, after the
// node has re-announced itself in response to riot2/orchestrator/online),
// fetches and parses {apiBaseUrl}api/Nodes/{id}/configuration into an
// in-memory NodeConfiguration, and notifies a callback so the rest of the
// app can rebuild itself from the new configuration without a full reboot.
// On success, also caches the raw response body via ConfigCache::save() so
// it can be reloaded offline on a later boot.
//
// requestConfiguration() performs a blocking HTTP(S) GET (bounded by a
// timeout), pinning the configured root CA for HTTPS URLs (see TlsRootCa.h),
// falling back to an insecure connection with a logged warning if none is
// configured. It only runs on the rare orchestrator (re)announce event, not
// on every loop() iteration, so blocking briefly is an acceptable trade-off
// for now; revisit if it turns out to starve MQTT/UI processing in practice.
class OrchestratorClient {
public:
    using ConfigurationCallback = std::function<void(const NodeConfiguration&)>;

    void onConfigurationUpdated(ConfigurationCallback callback) { _callback = callback; }

    const NodeConfiguration& current() const { return _current; }

    // Mounts ConfigCache and, if a previously-saved configuration exists,
    // parses it via the same parseConfiguration() path as a live fetch,
    // updates current(), and invokes the configuration callback - all
    // without any network access. Lets the UI rebuild from the last-known
    // configuration within seconds of power-on instead of waiting on
    // Wi-Fi/MQTT/the orchestrator. Returns false (no log - expected on a
    // genuine first boot) if no cache exists or it fails to parse, leaving
    // current() untouched.
    bool loadCached();

    // Fetches and parses the configuration for nodeId from apiBaseUrl. Returns
    // false (and logs) on any HTTP/JSON error, leaving the previous
    // configuration untouched. On success, updates current() and invokes the
    // configuration callback.
    bool requestConfiguration(const String& apiBaseUrl, const String& nodeId);

private:
    static constexpr uint32_t kHttpTimeoutMs = 8000;

    ConfigurationCallback _callback;
    NodeConfiguration _current;

    bool parseConfiguration(const String& json, NodeConfiguration& out);
};
