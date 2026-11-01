#include "riot2/CommandResults.h"

namespace riot2 {

// C++14 (gnu++14) needs this definition when the constant is bound by reference.
constexpr size_t CorrelationRing::kCapacity;

const CorrelationRing::Entry* CorrelationRing::find(const String& correlationId) const {
    if (correlationId.length() == 0) return nullptr;
    for (const Entry& entry : _entries) {
        if (entry.correlationId == correlationId) return &entry;
    }
    return nullptr;
}

void CorrelationRing::remember(const String& correlationId, const char* status, const String& error) {
    if (correlationId.length() == 0) return;
    Entry& entry = _entries[_next];
    entry.correlationId = correlationId;
    entry.status = status;
    entry.error = error;
    _next = (_next + 1) % kCapacity;
}

void CorrelationRing::forget(const String& correlationId) {
    if (correlationId.length() == 0) return;
    for (Entry& entry : _entries) {
        if (entry.correlationId == correlationId) {
            entry = Entry();
        }
    }
}

const char* CorrelationRing::redeliveryStatus(const Entry& entry) {
    return entry.status == CommandResultStatus::Executed ? CommandResultStatus::Duplicate : entry.status.c_str();
}

void writeCommandResult(JsonDocument& doc, const String& correlationId, const String& commandId,
                        const char* status, const String& error, uint32_t timeStamp) {
    doc.clear();
    doc["correlationId"] = correlationId;
    doc["commandId"] = commandId;
    doc["status"] = status;
    if (error.length() > 0) doc["error"] = error;
    doc["timeStamp"] = timeStamp;
}

}  // namespace riot2
