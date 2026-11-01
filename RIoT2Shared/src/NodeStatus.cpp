#include "riot2/NodeStatus.h"

#include <cstring>

namespace riot2 {

// C++14 (gnu++14) needs these definitions when a constant is bound by reference.
constexpr size_t NodeStatus::kMaxFailedDevices;
constexpr size_t NodeStatus::kMaxErrorLength;
constexpr long NodeStatus::kNoRevision;

bool NodeStatus::isApplied(const String& hash) const {
    return hash.length() > 0 && hash == _appliedHash && std::strcmp(_state, NodeConfigurationState::Applied) == 0;
}

void NodeStatus::applied(long revision, const String& hash, const std::vector<FailedDevice>& failedDevices) {
    _state = NodeConfigurationState::Applied;
    _hasApplied = true;
    _appliedRevision = revision;
    _appliedHash = hash;
    _error = String();
    _failedDevices = failedDevices;
}

bool NodeStatus::failed(const String& error) {
    String text = error.length() > 0 ? error : String("The configuration could not be applied.");
    if (text.length() > kMaxErrorLength) text = text.substring(0, kMaxErrorLength);
    if (std::strcmp(_state, NodeConfigurationState::Failed) == 0 && _error == text) return false;
    _state = NodeConfigurationState::Failed;
    _error = text;
    return true;
}

void NodeStatus::write(JsonDocument& doc, uint32_t timeStamp, size_t maxBytes) const {
    size_t devices = _failedDevices.size() < kMaxFailedDevices ? _failedDevices.size() : kMaxFailedDevices;
    fill(doc, timeStamp, devices);
    while (devices > 0 && measureJson(doc) > maxBytes) {
        fill(doc, timeStamp, --devices);
    }
}

void NodeStatus::fill(JsonDocument& doc, uint32_t timeStamp, size_t failedDevices) const {
    doc.clear();
    if (_appliedRevision != kNoRevision) doc["appliedRevision"] = _appliedRevision;
    if (_appliedHash.length() > 0) doc["appliedHash"] = _appliedHash;
    doc["state"] = _state;
    if (_error.length() > 0) doc["error"] = _error;
    if (failedDevices > 0) {
        JsonArray list = doc["failedDevices"].to<JsonArray>();
        for (size_t i = 0; i < failedDevices; ++i) {
            JsonObject device = list.add<JsonObject>();
            device["id"] = _failedDevices[i].id;
            device["message"] = _failedDevices[i].message;
        }
    }
    doc["timeStamp"] = timeStamp;
}

}  // namespace riot2
