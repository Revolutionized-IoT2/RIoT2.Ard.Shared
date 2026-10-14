#include "riot2/views/BleModel.h"

#include <ArduinoJson.h>

#include <cctype>
#include <vector>

namespace riot2 {
namespace views {

namespace {
// Resolves which reportTemplate (if any) should be used for one of the three report scenarios:
// prefers an exact match on `address`, falling back to the given positional index so a minimal
// configuration that just lists up to three reportTemplates in order (without bothering to set
// `address`) still works. Returns a pointer into `templates` (not a copy) so callers can also read
// that reportTemplate's own `parameters` (its `allowedAddresses` filter); nullptr if no
// match/fallback exists.
const ReportTemplate* resolveBleReport(const std::vector<ReportTemplate>& templates, const char* address,
                                       size_t fallbackIndex) {
    for (const auto& tmpl : templates) {
        if (tmpl.address == address) {
            return &tmpl;
        }
    }
    return fallbackIndex < templates.size() ? &templates[fallbackIndex] : nullptr;
}

// Uses ArduinoJson's serializer (rather than hand-built string concatenation) so the advertised
// `name` - untrusted data broadcast over the air by whatever nearby device - is always properly
// JSON-escaped rather than risking malformed/injected report payloads.
String bleJson(const JsonDocument& doc) {
    std::vector<char> buffer(measureJson(doc) + 1);
    serializeJson(doc, buffer.data(), buffer.size());
    return String(buffer.data());
}
}  // namespace

void BleModel::begin(const DeviceConfiguration& config) {
    _header = findParameter(config.deviceParameters, "header", "Nearby BLE");

    struct Binding {
        Channel& channel;
        const char* address;
        size_t fallbackIndex;
    };
    for (const Binding& binding : {Binding{_deviceFound, "deviceFound", 0}, Binding{_deviceLost, "deviceLost", 1},
                                   Binding{_advertisement, "advertisement", 2}}) {
        const ReportTemplate* tmpl = resolveBleReport(config.reportTemplates, binding.address, binding.fallbackIndex);
        binding.channel.reportId = tmpl ? tmpl->id : String();
        // The filter is read from that reportTemplate's `parameters`, not deviceParameters.
        binding.channel.allowedAddresses =
            tmpl ? splitAddressList(findParameter(tmpl->parameters, "allowedAddresses", "")) : std::vector<String>();
    }

    _devices.clear();
}

bool BleModel::isAddressAllowed(const std::vector<String>& allowedAddresses, const String& address) {
    if (allowedAddresses.empty()) {
        return true;
    }
    for (const auto& allowed : allowedAddresses) {
        if (allowed.equalsIgnoreCase(address)) {
            return true;
        }
    }
    return false;
}

std::vector<String> BleModel::splitAddressList(const String& raw) {
    std::vector<String> result;
    size_t start = 0;
    while (start <= raw.length()) {
        size_t end = start;
        while (end < raw.length() && raw[end] != ',') {
            ++end;
        }
        size_t first = start;
        size_t last = end;
        while (first < last && std::isspace(static_cast<unsigned char>(raw[first]))) {
            ++first;
        }
        while (last > first && std::isspace(static_cast<unsigned char>(raw[last - 1]))) {
            --last;
        }
        String token;
        for (size_t i = first; i < last; ++i) {
            token += raw[i];
        }
        if (token.length() > 0) {
            result.push_back(token);
        }
        start = end + 1;
    }
    return result;
}

void BleModel::onSnapshot(const std::vector<BleDeviceInfo>& devices) {
    _devices = devices;
}

bool BleModel::onDeviceDiscovered(const BleDeviceInfo& device, Report& report) {
    _devices.push_back(device);
    if (!_deviceFound.reports(device.address)) {
        return false;
    }
    JsonDocument doc;
    doc["address"] = device.address.c_str();
    doc["name"] = device.name.c_str();
    doc["rssi"] = device.rssi;
    report = Report{_deviceFound.reportId, bleJson(doc)};
    return true;
}

bool BleModel::onDeviceLost(const String& address, Report& report) {
    for (size_t i = 0; i < _devices.size(); ++i) {
        if (_devices[i].address == address) {
            _devices.erase(_devices.begin() + i);
            break;
        }
    }
    if (!_deviceLost.reports(address)) {
        return false;
    }
    report = Report{_deviceLost.reportId, stringLiteral(address)};
    return true;
}

bool BleModel::onAdvertisement(const BleAdvertisement& advertisement, Report& report) {
    if (!_advertisement.reports(advertisement.address)) {
        return false;
    }
    JsonDocument doc;
    doc["address"] = advertisement.address.c_str();
    doc["name"] = advertisement.name.c_str();
    doc["rssi"] = advertisement.rssi;
    doc["manufacturerData"] = advertisement.manufacturerDataHex.c_str();
    report = Report{_advertisement.reportId, bleJson(doc)};
    return true;
}

DeviceConfiguration BleModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "BLE View");
    config.deviceParameters = {{"header", "Nearby BLE"}};
    const char* const entries[][2] = {
        {"deviceFound", "BLE Device Found"}, {"deviceLost", "BLE Device Lost"}, {"advertisement", "BLE Advertisement"}};
    for (const auto& entry : entries) {
        ReportTemplate report = makeReport(entry[1], entry[0], riot2::ValueType::Text);
        report.parameters = {{"allowedAddresses", ""}};
        config.reportTemplates.push_back(report);
    }
    return config;
}

}  // namespace views
}  // namespace riot2
