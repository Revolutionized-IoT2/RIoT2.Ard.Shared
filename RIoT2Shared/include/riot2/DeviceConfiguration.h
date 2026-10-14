#pragma once

#include <Arduino.h>

#include <utility>
#include <vector>

// In-memory model for the device configuration fetched from the RIoT2
// Orchestrator (GET {apiBaseUrl}api/Nodes/{id}/configuration). Mirrors
// RIoT2.Core.Models (CommandTemplate, ReportTemplate, DeviceConfiguration).

// Free-form string dictionary helper shared by ReportTemplate/DeviceConfiguration.
using ParameterList = std::vector<std::pair<String, String>>;

namespace riot2 {
// RIoT2.Core.Enums.ValueType: the `type` of a command or report template, a number on the wire.
namespace ValueType {
constexpr int Boolean = 0;
constexpr int Text = 1;
constexpr int Number = 2;
constexpr int Entity = 3;
constexpr int TextArray = 4;
}  // namespace ValueType
}  // namespace riot2

struct CommandTemplate {
    String id;
    int type = riot2::ValueType::Boolean;
    String name;
    String address;
    bool model = false;
};

inline String findParameter(const ParameterList& params, const String& key, const String& defaultValue = "") {
    for (const auto& kv : params) {
        if (kv.first == key) {
            return kv.second;
        }
    }
    return defaultValue;
}

struct ReportTemplate {
    String id;
    int type = riot2::ValueType::Boolean;
    String name;
    String address;
    ParameterList parameters;
};

struct DeviceConfiguration {
    String id;
    String name;
    String classFullName;
    std::vector<CommandTemplate> commandTemplates;
    std::vector<ReportTemplate> reportTemplates;
    ParameterList deviceParameters;
};

struct NodeConfiguration {
    String name;
    String id;

    // Both on-screen Views and non-visual Grove-port peripherals (see
    // IPeripheral/PeripheralManager) live in this same list, distinguished
    // purely by classFullName: ViewManager resolves entries via
    // ViewFactory, PeripheralManager resolves entries via PeripheralFactory
    // - each manager silently skips entries the other one owns.
    std::vector<DeviceConfiguration> deviceConfigurations;
};
