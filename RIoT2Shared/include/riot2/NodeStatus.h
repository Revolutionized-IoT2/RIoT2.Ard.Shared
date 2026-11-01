#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <vector>

// The node's view of its configuration (design 7.2, phase 5), which NodeRuntime publishes retained
// on riot2/node/{id}/status on every connect (before the presence) and on every change. Mirrors
// RIoT2.Core's NodeConfigurationStatus and NodeStatusMessage, in a compact form: firmware has no
// plugins, so there is no pluginManifest.
namespace riot2 {

namespace NodeConfigurationState {
constexpr const char* Applying = "applying";
constexpr const char* Applied = "applied";
constexpr const char* Failed = "failed";
}  // namespace NodeConfigurationState

struct FailedDevice {
    String id;
    String message;
};

class NodeStatus {
public:
    // Design 7.2: at most four failed devices, to stay well under MQTT_MAX_PACKET_SIZE.
    static constexpr size_t kMaxFailedDevices = 4;
    static constexpr size_t kMaxErrorLength = 120;
    static constexpr long kNoRevision = -1;

    // Until the first configuration is applied the state is `applying`, without a revision.
    const char* state() const { return _state; }
    const String& appliedHash() const { return _appliedHash; }
    bool hasAppliedConfiguration() const { return _hasApplied; }

    // True when the configuration with `hash` is applied, so a configuration command for it needs
    // no fetch. Unlike the .NET node, failed devices don't count: on firmware they are classes
    // this build doesn't have, which fetching again cannot fix.
    bool isApplied(const String& hash) const;

    // A configuration was fetched and applied; `revision` is kNoRevision and `hash` empty when an
    // orchestrator before 1.5.0 served it. Devices that didn't start are listed, but the
    // configuration itself counts as applied (design 7.2, decision 5).
    void applied(long revision, const String& hash, const std::vector<FailedDevice>& failedDevices);

    // The configuration could not be fetched or applied; the node keeps the previous one, and its
    // revision and hash stay. Returns false when the status already said exactly this.
    bool failed(const String& error);

    // The NodeStatusMessage. Failed devices beyond kMaxFailedDevices are left out, and more of
    // them while the document is longer than maxBytes.
    void write(JsonDocument& doc, uint32_t timeStamp, size_t maxBytes) const;

private:
    const char* _state = NodeConfigurationState::Applying;
    bool _hasApplied = false;
    long _appliedRevision = kNoRevision;
    String _appliedHash;
    String _error;
    std::vector<FailedDevice> _failedDevices;

    void fill(JsonDocument& doc, uint32_t timeStamp, size_t failedDevices) const;
};

}  // namespace riot2
