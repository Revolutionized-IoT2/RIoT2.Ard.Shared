#include <algorithm>
#include <cassert>
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
// PRODUCTION_WIFI_METHODS
WifiConnection wifi;
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
bool bleActive = false;
void configureCanvasColorDepth() {}
void applyConfiguration() {
    // PRODUCTION_BLE_ACTIVATION
}
int main() {
    wifi.begin("ssid", "password");
    applyConfiguration();
    wifi.loop();
    assert(!bleActive && !WiFi.sleep && WiFi.begins == 1);
    WiFi.statusValue = WL_CONNECTED;
    wifi.loop();
    viewManager.consumer = true;
    applyConfiguration();
    assert(bleActive && bleRunning && WiFi.sleep);
    WiFi.statusValue = 0;
    wifi.loop(); // Records disconnect.
    nowMs += 30001;
    wifi.loop(); // Retry with active BLE.
    assert(WiFi.sleep && WiFi.begins == 2);
    nowMs += 15001;
    wifi.loop(); // Connection times out.
    nowMs += 30001;
    wifi.loop(); // Subsequent retry must also preserve sleep.
    assert(WiFi.sleep && WiFi.begins == 3);
    viewManager.consumer = false;
    applyConfiguration(); // Scanner stays active after removal; retain compatible policy.
    nowMs += 15001;
    wifi.loop();
    nowMs += 30001;
    wifi.loop();
    assert(WiFi.sleep && WiFi.begins == 4);
}
