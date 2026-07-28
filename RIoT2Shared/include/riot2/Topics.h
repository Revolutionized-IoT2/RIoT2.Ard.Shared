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

inline const char* orchestratorOnline() {
    return "riot2/orchestrator/online";
}

}  // namespace Topics
