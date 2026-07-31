#pragma once

#include <Arduino.h>

#include <vector>

#include <riot2/DeviceConfiguration.h>

namespace riot2 {

// Serializes a list of DeviceConfiguration templates to a JSON array string
// matching RIoT2.Core.Models.DeviceConfiguration's wire format (see
// OrchestratorClient::parseConfiguration() for the same field names read in
// reverse) - the body of the /api/device/configuration/templates response
// (see ConfigTemplateServer.h).
String serializeDeviceConfigurations(const std::vector<DeviceConfiguration>& configs);

}  // namespace riot2
