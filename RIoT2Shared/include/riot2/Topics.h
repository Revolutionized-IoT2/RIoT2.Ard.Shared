#pragma once

#include <Arduino.h>

// Centralised MQTT topic templates and JSON field names.
//
// Confirmed against RIoT2.Core.Constants (../RIoT2.Core/Constants.cs): the
// orchestrator uses a "node/" segment for per-node topics, so these match
// nodeOnlineTopic/commandTopic/reportTopic = "riot2/node/{id}/...".
namespace Topics {

inline String online(const String& nodeId) {
    return "riot2/node/" + nodeId + "/online";
}

inline String command(const String& nodeId) {
    return "riot2/node/" + nodeId + "/command";
}

inline String report(const String& nodeId) {
    return "riot2/node/" + nodeId + "/report";
}

inline String configuration(const String& nodeId) {
    return "riot2/node/" + nodeId + "/configuration";
}

// Reserved by RIoT2.Core 1.6.0 for command results (design 7.1) and the retained configuration
// status (design 7.2). The firmware publishes on them once it supports those features.
inline String commandResult(const String& nodeId) {
    return "riot2/node/" + nodeId + "/command/result";
}

inline String status(const String& nodeId) {
    return "riot2/node/" + nodeId + "/status";
}

inline const char* orchestratorOnline() {
    return "riot2/orchestrator/online";
}

}  // namespace Topics
