#pragma once

#include <Arduino.h>

// Persists the most recently fetched raw orchestrator configuration JSON to
// flash (LittleFS) so the device can rebuild its UI/peripherals offline
// immediately after boot, before Wi-Fi/MQTT/the orchestrator are reachable
// again - reusing the exact same parse path as a live fetch (see
// OrchestratorClient::parseConfiguration), since the cached bytes are the
// same raw JSON body, not a second serialization format.
namespace ConfigCache {

// Mounts LittleFS if not already mounted. Safe to call more than once.
void begin();

// Overwrites the cached configuration. No-op (logs) on any filesystem error.
void save(const String& rawJson);

// Returns true and fills `out` if a cached configuration exists and could be
// read; false (and leaves `out` untouched) otherwise.
bool tryLoad(String& out);

}  // namespace ConfigCache
