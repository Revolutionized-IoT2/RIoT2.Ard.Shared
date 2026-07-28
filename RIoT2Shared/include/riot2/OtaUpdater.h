#pragma once

#include <Arduino.h>

// OTA firmware update over HTTP(S), triggered by a reserved MQTT command
// (main.cpp's handleCommand - command.id == "system.ota"). This is a
// RIoT2 Ard node-specific extension, not part of the RIoT2.Core
// Command/Report contract: the orchestrator (or an operator) publishes
// { "id": "system.ota", "value": "http://host/firmware.bin" } to this
// node's riot2/node/{id}/command topic to trigger an update.
namespace OtaUpdater {

// Downloads the firmware binary at `url` and flashes it. Blocks for the
// duration of the download/flash (no MQTT/UI servicing meanwhile). On
// success the device reboots automatically and this function never returns;
// on failure it returns false and the current firmware keeps running.
bool performUpdate(const String& url);

}  // namespace OtaUpdater
