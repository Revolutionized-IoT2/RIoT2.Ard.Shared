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
// timeout and a 32 KiB response-body cap), pinning the configured root CA for
// HTTPS URLs (see TlsRootCa.h), falling back to an insecure connection with a
// logged warning if none is configured. It only runs on the rare orchestrator
// (re)announce event, not on every loop() iteration, so blocking briefly is an
// acceptable trade-off for now; revisit if it turns out to starve MQTT/UI
// processing in practice.
class OrchestratorClient {
public:
    using ConfigurationCallback = std::function<void(const NodeConfiguration&)>;

    // enableCache=false disables both reading (loadCached()) and writing
    // (the ConfigCache::save() call inside requestConfiguration()) of the
    // on-flash configuration cache entirely - for nodes that would rather
    // always show nothing until a live fetch succeeds than risk running
    // from a stale offline copy (see RIoT2.Ard.M5Core2.Node/src/main.cpp).
    // Defaults to true, preserving the original loadCached()-on-boot
    // behavior for existing consumers (e.g. RIoT2.Ard.M5Dial.Node).
    explicit OrchestratorClient(bool enableCache = true) : _cacheEnabled(enableCache) {}

    void onConfigurationUpdated(ConfigurationCallback callback) { _callback = callback; }

    const NodeConfiguration& current() const { return _current; }

    // No-op (returns false) when constructed with enableCache=false. Mounts
    // ConfigCache and, if a previously-saved configuration exists,
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
    //
    // If the fetched body is byte-for-byte identical to the last body that
    // was actually applied (via this method or loadCached()), the parse and
    // configuration callback are both skipped (still returns true, and still
    // refreshes the on-flash cache) - the orchestrator can re-deliver the
    // same configuration more than once (e.g. a retained MQTT message
    // re-triggering this on every reconnect), and re-running the callback
    // means the app fully rebuilds its UI (ViewManager::rebuild() tearing
    // down and recreating every view's LVGL widgets) for no actual change.
    // That rebuild is expensive enough to matter: on a 13-view
    // configuration, doing it twice back-to-back (once from loadCached(),
    // once from an unchanged live fetch) measured free heap dropping from
    // ~25KB to ~6KB, right at the edge of allocation failures elsewhere
    // (BLE/TLS/LVGL).
    bool requestConfiguration(const String& apiBaseUrl, const String& nodeId);

private:
    static constexpr uint32_t kHttpTimeoutMs = 8000;

    bool _cacheEnabled;
    ConfigurationCallback _callback;
    NodeConfiguration _current;
    String _lastAppliedJson;

    bool parseConfiguration(const String& json, NodeConfiguration& out);
};
