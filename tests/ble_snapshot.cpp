#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
struct String : std::string {
    using std::string::string;
    String() = default;
    String(const std::string& value) : std::string(value) {}
    bool equalsIgnoreCase(const String& other) const { return *this == other; }
};
unsigned long nowMs = 100;
unsigned long millis() { return nowMs; }
using SemaphoreHandle_t = void*;
constexpr int pdTRUE = 1, portMAX_DELAY = 0;
int xSemaphoreTake(SemaphoreHandle_t, int) { return pdTRUE; }
void xSemaphoreGive(SemaphoreHandle_t) {}
// PRODUCTION_TYPES
#define private public
// PRODUCTION_SCANNER_CLASS
#undef private
// PRODUCTION_SCANNER_METHODS
struct DeviceConfiguration {};
struct lv_obj_t {};
struct M5Canvas {};
struct Report { String id, value; };
std::vector<Report> reports;
String deviceJson(const String& address, const String&, int) { return address; }
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
void CoreBLEView::begin(const DeviceConfiguration&) {}
void CoreBLEView::buildUi(lv_obj_t*) {}
void CoreBLEView::onBleAdvertisement(const BleAdvertisement&) {}
void DialBLEView::begin(const DeviceConfiguration&) {}
void DialBLEView::render(M5Canvas&) {}
void DialBLEView::onEncoderChange(int) {}
bool DialBLEView::isInteracting() const { return false; }
void DialBLEView::onBleAdvertisement(const BleAdvertisement&) {}
void restoreCore(IView* view) {
    // RESTORE_CORE
}
void restoreDial(IView* view) {
    // RESTORE_DIAL
}
template<typename View>
void check(void (*restore)(IView*)) {
    auto& scanner = BleScanner::instance();
    scanner._began = true;
    scanner._devices.clear();
    scanner._pending.clear();
    reports.clear();
    nowMs = 100;
    auto view = std::unique_ptr<View>(new View);
    view->_deviceFoundReportId = "old-found";
    view->_deviceLostReportId = "lost";
    scanner.onDeviceDiscovered([&](const BleDeviceInfo& device) { view->onBleDeviceDiscovered(device); });
    scanner.onDeviceLost([&](const String& address) { view->onBleDeviceLost(address); });
    scanner._pending.push_back({"A", "Device A", -50, ""});
    scanner.loop();
    assert(view->_devices.size() == 1 && reports.size() == 1);
    view.reset(new View);
    view->_deviceFoundReportId = "new-found";
    view->_deviceLostReportId = "lost";
    restore(view.get());
    assert(view->_devices.size() == 1 && view->_devices[0].address == "A");
    assert(reports.size() == 1); // Snapshot must not retrigger discovery automation.
    ++nowMs;
    scanner._pending.push_back({"A", "Device A", -40, ""});
    scanner.loop();
    assert(view->_devices.size() == 1 && reports.size() == 1);
    scanner._pending.push_back({"B", "Device B", -60, ""});
    scanner.loop();
    assert(view->_devices.size() == 2 && reports.size() == 2 && reports.back().id == "new-found");
    nowMs += 59999;
    assert(scanner.snapshot().size() == 2);
    ++nowMs;
    assert(scanner.snapshot().empty()); // Exact 60-second expiration boundary.
    scanner.loop();
    assert(view->_devices.empty() && reports.size() == 4);
    scanner.onDeviceDiscovered(nullptr);
    scanner.onDeviceLost(nullptr);
}
int main() {
    check<CoreBLEView>(restoreCore);
    check<DialBLEView>(restoreDial);
    assert(refreshes > 0);
}
