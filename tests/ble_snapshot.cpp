#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <riot2/BleTypes.h>
#include <riot2/DeviceConfiguration.h>
#include <riot2/Report.h>
#include <riot2/Uuid.h>

unsigned long hostMillis = 100;
namespace riot2 { String newId() { static int n = 0; return String("id-") + String(++n); } }
// PRODUCTION_MODELS
using SemaphoreHandle_t = void*;
constexpr int pdTRUE = 1, portMAX_DELAY = 0;
int xSemaphoreTake(SemaphoreHandle_t, int) { return pdTRUE; }
void xSemaphoreGive(SemaphoreHandle_t) {}
#define private public
// PRODUCTION_SCANNER_CLASS
#undef private
// PRODUCTION_SCANNER_METHODS
struct lv_obj_t {};
struct M5Canvas {};
std::vector<Report> reports;
class IView {
public:
    virtual ~IView() = default;
    virtual void begin(const DeviceConfiguration&) {}
    virtual void buildUi(lv_obj_t*) {}
    virtual void render(M5Canvas&) {}
    virtual void onEncoderChange(int) {}
    virtual bool isInteracting() const { return false; }
    virtual bool consumesBleEvents() const { return false; }
    virtual void onBleSnapshot(const std::vector<BleDeviceInfo>&) {}
    virtual void onBleDeviceDiscovered(const BleDeviceInfo&) {}
    virtual void onBleDeviceLost(const String&) {}
    virtual void onBleAdvertisement(const BleAdvertisement&) {}
    void publishReport(const Report& report) { reports.push_back(report); }
};
#define private public
// PRODUCTION_CORE_CLASS
// PRODUCTION_DIAL_CLASS
#undef private
// PRODUCTION_CORE_METHODS
// PRODUCTION_DIAL_METHODS
unsigned refreshes = 0;
void CoreBLEView::refreshList() { ++refreshes; }
void CoreBLEView::buildUi(lv_obj_t*) {}
void DialBLEView::render(M5Canvas&) {}
void restoreCore(IView* view) {
    // RESTORE_CORE
}
void restoreDial(IView* view) {
    // RESTORE_DIAL
}
DeviceConfiguration bleConfig(const char* foundId) {
    DeviceConfiguration config;
    ReportTemplate found, lost;
    found.id = foundId;
    found.address = "deviceFound";
    lost.id = "lost";
    lost.address = "deviceLost";
    config.reportTemplates = {found, lost};
    return config;
}
template<typename View>
void check(void (*restore)(IView*)) {
    auto& scanner = BleScanner::instance();
    scanner._began = true;
    scanner._devices.clear();
    scanner._pending.clear();
    reports.clear();
    hostMillis = 100;
    auto view = std::unique_ptr<View>(new View);
    view->begin(bleConfig("old-found"));
    scanner.onDeviceDiscovered([&](const BleDeviceInfo& device) { view->onBleDeviceDiscovered(device); });
    scanner.onDeviceLost([&](const String& address) { view->onBleDeviceLost(address); });
    scanner.onAdvertisement([&](const BleAdvertisement& advertisement) { view->onBleAdvertisement(advertisement); });
    scanner._pending.push_back({"A", "Device A", -50, ""});
    scanner.loop();
    assert(view->_model.devices().size() == 1 && reports.size() == 1 && reports[0].id == "old-found");
    view.reset(new View);
    view->begin(bleConfig("new-found"));
    restore(view.get());
    assert(view->_model.devices().size() == 1 && view->_model.devices()[0].address == "A");
    assert(reports.size() == 1); // Snapshot must not retrigger discovery automation.
    ++hostMillis;
    scanner._pending.push_back({"A", "Device A", -40, ""});
    scanner.loop();
    assert(view->_model.devices().size() == 1 && reports.size() == 1);
    scanner._pending.push_back({"B", "Device B", -60, ""});
    scanner.loop();
    assert(view->_model.devices().size() == 2 && reports.size() == 2 && reports.back().id == "new-found");
    hostMillis += 59999;
    assert(scanner.snapshot().size() == 2);
    ++hostMillis;
    assert(scanner.snapshot().empty()); // Exact 60-second expiration boundary.
    scanner.loop();
    assert(view->_model.devices().empty() && reports.size() == 4);
    assert(reports[2].id == "lost" && reports[3].id == "lost" && reports[3].value == "\"B\"");
    scanner.onDeviceDiscovered(nullptr);
    scanner.onDeviceLost(nullptr);
    scanner.onAdvertisement(nullptr);
}
int main() {
    check<CoreBLEView>(restoreCore);
    check<DialBLEView>(restoreDial);
    assert(refreshes > 0);

    // Dial keeps its scroll position within the list as devices come and go.
    DialBLEView dial;
    dial.begin(bleConfig("found"));
    std::vector<BleDeviceInfo> devices(6);
    for (size_t i = 0; i < devices.size(); ++i) devices[i].address = String(static_cast<int>(i));
    dial.onBleSnapshot(devices);
    assert(dial.isInteracting());
    for (int i = 0; i < 5; ++i) dial.onEncoderChange(1);
    assert(dial._scrollOffset == 2);
    dial.onBleDeviceLost("0");
    assert(dial._scrollOffset == 1);
    dial.onBleSnapshot({});
    assert(dial._scrollOffset == 0 && !dial.isInteracting());
}