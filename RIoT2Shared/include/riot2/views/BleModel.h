#pragma once

#include <Arduino.h>

#include <vector>

#include <riot2/BleTypes.h>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// Nearby BLE advertisers from BleScanner (see BleScanner.h), fed in whether or not the view is
// shown, plus the reports for them. deviceParameter "header" (default "Nearby BLE").
//
// Reports are published for three distinct scenarios, each addressed by a separate
// reportTemplate matched by its `address` field (falling back to positional order 0/1/2 if no
// template has that address):
//   - address "deviceFound"   -> a previously-unseen device started advertising
//   - address "deviceLost"    -> a previously-seen device is no longer present
//   - address "advertisement" -> forwarded for every advertisement received
// Any of the three may be omitted from the configuration; that scenario is then simply not
// reported (the device list keeps working regardless).
//
// Optional per-reportTemplate `parameters` entry `allowedAddresses`: a comma-separated list of
// BLE MAC addresses (matched case-insensitively) that THIS report should be restricted to - e.g.
// "AA:BB:CC:DD:EE:FF, 11:22:33:44:55:66" on the "deviceFound" reportTemplate. Each report has its
// own independent filter. Devices excluded by a report's filter are still tracked and listed,
// they just don't publish that report. Without `allowedAddresses` (or blank) nothing is filtered.
class BleModel {
public:
    void begin(const DeviceConfiguration& config);

    const String& header() const { return _header; }
    // Present devices, in discovery order.
    const std::vector<BleDeviceInfo>& devices() const { return _devices; }

    // Replaces the list with the scanner's current devices (after a rebuild); never reports, so
    // a snapshot doesn't retrigger discovery automation.
    void onSnapshot(const std::vector<BleDeviceInfo>& devices);
    // Each adds/removes the device and returns true with the report to publish, if any.
    bool onDeviceDiscovered(const BleDeviceInfo& device, Report& report);
    bool onDeviceLost(const String& address, Report& report);
    bool onAdvertisement(const BleAdvertisement& advertisement, Report& report);

    // An empty list allows every address.
    static bool isAddressAllowed(const std::vector<String>& allowedAddresses, const String& address);
    // Trimmed, non-empty tokens of a comma-separated address list.
    static std::vector<String> splitAddressList(const String& raw);

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    struct Channel {
        String reportId;
        std::vector<String> allowedAddresses;
        bool reports(const String& address) const {
            return reportId.length() > 0 && isAddressAllowed(allowedAddresses, address);
        }
    };

    String _header;
    Channel _deviceFound;
    Channel _deviceLost;
    Channel _advertisement;
    std::vector<BleDeviceInfo> _devices;
};

}  // namespace views
}  // namespace riot2
