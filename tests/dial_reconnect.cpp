#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <vector>
using String = std::string;
using std::min;
unsigned long nowMs = 1;
unsigned long millis() { return nowMs; }
struct Log {
    template <typename... Args> void printf(const char*, Args...) {}
    void println(const char*) {}
} Serial;
constexpr int WIFI_STA = 1, WL_CONNECTED = 3;
bool bleRunning = false;
struct WifiFake {
    int statusValue = 0;
    bool sleep = true;
    unsigned begins = 0;
    void mode(int) {}
    void setSleep(bool enabled) {
        assert(!bleRunning || enabled);
        sleep = enabled;
    }
    void begin(const char*, const char*) { ++begins; }
    int status() { return statusValue; }
    struct IP { String toString() { return "192.0.2.1"; } };
    IP localIP() { return {}; }
} WiFi;
// PRODUCTION_WIFI_CLASS
constexpr unsigned long WifiConnection::kInitialBackoffMs;
constexpr unsigned long WifiConnection::kMaxBackoffMs;
// PRODUCTION_WIFI_METHODS
// NodeRuntime's members, as the spliced code uses them.
WifiConnection _wifi;
struct Hooks {
    std::function<void()> onBleStarted;
} _hooks;
unsigned bleStarts = 0;
struct Views {
    bool consumer = false;
    bool hasBleConsumer() { return consumer; }
} viewManager;
struct BleScanner {
    static BleScanner& instance() { static BleScanner scanner; return scanner; }
    void begin() {
        WiFi.setSleep(true); // Matches scanner's initial WIFI_PS_MIN_MODEM.
        bleRunning = true;
    }
};
bool _bleActive = false;
void applyConfiguration() {
    bool wantsBle = viewManager.hasBleConsumer();
    // PRODUCTION_BLE_ACTIVATION
}
int main() {
    _hooks.onBleStarted = [] { ++bleStarts; };
    _wifi.begin("ssid", "password");
    applyConfiguration();
    _wifi.loop();
    assert(!_bleActive && !WiFi.sleep && WiFi.begins == 1);
    WiFi.statusValue = WL_CONNECTED;
    _wifi.loop();
    viewManager.consumer = true;
    applyConfiguration();
    assert(_bleActive && bleRunning && WiFi.sleep && bleStarts == 1);
    applyConfiguration();
    assert(bleStarts == 1);  // the scanner starts once
    WiFi.statusValue = 0;
    _wifi.loop(); // Records disconnect.
    nowMs += 30001;
    _wifi.loop(); // Retry with active BLE.
    assert(WiFi.sleep && WiFi.begins == 2);
    nowMs += 15001;
    _wifi.loop(); // Connection times out.
    nowMs += 30001;
    _wifi.loop(); // Subsequent retry must also preserve sleep.
    assert(WiFi.sleep && WiFi.begins == 3);
    viewManager.consumer = false;
    applyConfiguration(); // Scanner stays active after removal; retain compatible policy.
    nowMs += 15001;
    _wifi.loop();
    nowMs += 30001;
    _wifi.loop();
    assert(WiFi.sleep && WiFi.begins == 4);
}
