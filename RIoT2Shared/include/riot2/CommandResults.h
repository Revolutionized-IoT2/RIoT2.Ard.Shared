#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>

// Command results (design 7.1, phase 3): what NodeRuntime answers on riot2/node/{id}/command/result
// for a command that carries a correlationId. Mirrors RIoT2.Core's CommandResult and the .NET
// node's CorrelationCache.
namespace riot2 {

// Advertised in the NodeOnlineMessage's `capabilities` (RIoT2.Core Contract.Capabilities).
namespace Capabilities {
constexpr const char* CommandResult = "command-result/1";
constexpr const char* DesiredState = "desired-state/1";
}  // namespace Capabilities

namespace CommandResultStatus {
constexpr const char* Executed = "executed";
constexpr const char* Failed = "failed";
constexpr const char* Rejected = "rejected";
constexpr const char* UnknownCommand = "unknownCommand";
constexpr const char* Duplicate = "duplicate";
}  // namespace CommandResultStatus

// The outcomes of the last kCapacity correlated commands, oldest overwritten first. The firmware
// runs a command to completion inside the MQTT callback, so unlike the .NET cache there is no
// "still running" entry.
class CorrelationRing {
public:
    static constexpr size_t kCapacity = 16;

    struct Entry {
        String correlationId;
        String status;
        String error;
    };

    // nullptr when the id is not remembered.
    const Entry* find(const String& correlationId) const;
    void remember(const String& correlationId, const char* status, const String& error);
    // A rejected command did not run: the orchestrator's next delivery must run it.
    void forget(const String& correlationId);

    // What a redelivery of a remembered command gets: `duplicate` for one that executed, the
    // original status and error otherwise.
    static const char* redeliveryStatus(const Entry& entry);

private:
    Entry _entries[kCapacity];
    size_t _next = 0;
};

// {"correlationId","commandId","status","error"?,"timeStamp"}; `error` only when not empty.
void writeCommandResult(JsonDocument& doc, const String& correlationId, const String& commandId,
                        const char* status, const String& error, uint32_t timeStamp);

}  // namespace riot2
