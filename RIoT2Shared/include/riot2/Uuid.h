#pragma once

#include <Arduino.h>

namespace riot2 {

// Generates a random RFC 4122 v4 UUID string ("xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"),
// using the ESP32's hardware RNG (esp_random()) - available even before
// Wi-Fi is up. Used to mint fresh, unique ids for the illustrative
// commandTemplates/reportTemplates/deviceConfigurations returned by
// /api/device/configuration/templates (see ConfigTemplateServer.h), mirroring
// the Guid.NewGuid() ids RIoT2.Net.Devices' IDeviceWithConfiguration
// implementations use for the same purpose.
String newId();

}  // namespace riot2
