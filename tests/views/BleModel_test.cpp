#include "view_test.h"

using namespace riot2::views;

namespace {
BleDeviceInfo bleDevice(const char* address, const char* name, int rssi) {
    BleDeviceInfo device;
    device.address = address;
    device.name = name;
    device.rssi = rssi;
    return device;
}
}  // namespace

int main() {
    auto core = BleModel::buildTemplate("RIoT2.Ard.M5Core2.Node.BLEView");
    CHECK(core.name == "BLE View" && core.classFullName == "RIoT2.Ard.M5Core2.Node.BLEView");
    CHECK(core.deviceParameters.size() == 1 && core.deviceParameters[0].first == "header" &&
          core.deviceParameters[0].second == "Nearby BLE");
    CHECK(core.commandTemplates.empty() && core.reportTemplates.size() == 3);
    const char* addresses[] = {"deviceFound", "deviceLost", "advertisement"};
    const char* names[] = {"BLE Device Found", "BLE Device Lost", "BLE Advertisement"};
    for (size_t i = 0; i < 3; ++i) {
        const ReportTemplate& report = core.reportTemplates[i];
        CHECK(report.address == addresses[i] && report.name == names[i] && report.type == riot2::ValueType::Text);
        CHECK(report.parameters.size() == 1 && report.parameters[0].first == "allowedAddresses" &&
              report.parameters[0].second == "");
    }
    auto dial = BleModel::buildTemplate("RIoT2.Ard.M5Dial.Node.BLEView");
    CHECK(dial.classFullName == "RIoT2.Ard.M5Dial.Node.BLEView" && dial.reportTemplates.size() == 3);
    CHECK(dial.id != core.id && dial.reportTemplates[0].id != core.reportTemplates[0].id);

    // Address lists: trimmed, non-empty tokens; matched case-insensitively; empty allows all.
    auto list = BleModel::splitAddressList(" AA:BB , ,cc:dd,\t,ee ");
    CHECK(list.size() == 3 && list[0] == "AA:BB" && list[1] == "cc:dd" && list[2] == "ee");
    CHECK(BleModel::splitAddressList("").empty() && BleModel::splitAddressList(" , ").empty());
    CHECK(BleModel::splitAddressList("a,").size() == 1);
    CHECK(BleModel::isAddressAllowed({}, "anything"));
    CHECK(BleModel::isAddressAllowed(list, "aa:bb") && BleModel::isAddressAllowed(list, "CC:DD"));
    CHECK(!BleModel::isAddressAllowed(list, "aa:bb:cc"));

    BleModel model;
    model.begin(dial);
    CHECK(model.header() == "Nearby BLE" && model.devices().empty());
    const String found = dial.reportTemplates[0].id, lost = dial.reportTemplates[1].id,
                 advertised = dial.reportTemplates[2].id;

    Report report;
    CHECK(model.onDeviceDiscovered(bleDevice("aa:bb:cc:dd:ee:01", "Tag \"1\"", -50), report));
    CHECK(report.id == found && report.value == "{\"address\":\"aa:bb:cc:dd:ee:01\",\"name\":\"Tag \\\"1\\\"\",\"rssi\":-50}");
    CHECK(model.onDeviceDiscovered(bleDevice("aa:bb:cc:dd:ee:02", "", -70), report));
    CHECK(report.value == "{\"address\":\"aa:bb:cc:dd:ee:02\",\"name\":\"\",\"rssi\":-70}");
    CHECK(model.devices().size() == 2 && model.devices()[1].address == "aa:bb:cc:dd:ee:02");

    BleAdvertisement advertisement;
    advertisement.address = "aa:bb:cc:dd:ee:01";
    advertisement.name = "Tag";
    advertisement.rssi = -42;
    advertisement.manufacturerDataHex = "4C00";
    CHECK(model.onAdvertisement(advertisement, report) && report.id == advertised);
    CHECK(report.value ==
          "{\"address\":\"aa:bb:cc:dd:ee:01\",\"name\":\"Tag\",\"rssi\":-42,\"manufacturerData\":\"4C00\"}");
    CHECK(model.devices().size() == 2);

    CHECK(model.onDeviceLost("aa:bb:cc:dd:ee:01", report) && report.id == lost && report.value == "\"aa:bb:cc:dd:ee:01\"");
    CHECK(model.devices().size() == 1 && model.devices()[0].address == "aa:bb:cc:dd:ee:02");
    // An unknown device still reports its loss; the list doesn't change.
    CHECK(model.onDeviceLost("unknown", report) && report.value == "\"unknown\"" && model.devices().size() == 1);

    // A snapshot replaces the list without reporting.
    report = Report{"untouched", "x"};
    model.onSnapshot({bleDevice("11", "One", -1), bleDevice("22", "Two", -2), bleDevice("33", "", -3)});
    CHECK(model.devices().size() == 3 && model.devices()[2].address == "33" && report.id == "untouched");

    // Per-report filters: excluded devices are still listed, they just don't report.
    DeviceConfiguration filtered = dial;
    filtered.deviceParameters = {{"header", "Beacons"}};
    filtered.reportTemplates[0].parameters = {{"allowedAddresses", "AA:BB:CC:DD:EE:01, 11:22:33:44:55:66"}};
    filtered.reportTemplates[2].parameters = {{"allowedAddresses", "11:22:33:44:55:66"}};
    model.begin(filtered);
    CHECK(model.header() == "Beacons" && model.devices().empty());
    CHECK(model.onDeviceDiscovered(bleDevice("aa:bb:cc:dd:ee:01", "A", -1), report) && report.id == found);
    CHECK(!model.onDeviceDiscovered(bleDevice("aa:bb:cc:dd:ee:02", "B", -1), report));
    CHECK(model.devices().size() == 2);
    CHECK(!model.onAdvertisement(advertisement, report));
    CHECK(model.onDeviceLost("aa:bb:cc:dd:ee:02", report) && report.id == lost && model.devices().size() == 1);

    // Templates resolve by address first, then by position; missing ones don't report.
    DeviceConfiguration reordered = dial;
    reordered.reportTemplates = {dial.reportTemplates[2], dial.reportTemplates[1], dial.reportTemplates[0]};
    model.begin(reordered);
    CHECK(model.onDeviceDiscovered(bleDevice("1", "", 0), report) && report.id == found);
    CHECK(model.onAdvertisement(advertisement, report) && report.id == advertised);
    DeviceConfiguration positional = dial;
    for (auto& tmpl : positional.reportTemplates) {
        tmpl.address = "";
    }
    positional.reportTemplates.pop_back();
    model.begin(positional);
    CHECK(model.onDeviceDiscovered(bleDevice("1", "", 0), report) && report.id == found);
    CHECK(model.onDeviceLost("1", report) && report.id == lost);
    CHECK(!model.onAdvertisement(advertisement, report));
    model.begin(DeviceConfiguration());
    CHECK(model.header() == "Nearby BLE");
    CHECK(!model.onDeviceDiscovered(bleDevice("1", "", 0), report) && model.devices().size() == 1);
    CHECK(!model.onDeviceLost("1", report) && model.devices().empty());
    std::printf("BleModel ok\n");
}
