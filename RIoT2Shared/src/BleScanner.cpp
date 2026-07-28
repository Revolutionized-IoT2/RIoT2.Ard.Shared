#include <riot2/BleScanner.h>

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {

// Hex-encodes an arbitrary byte string (e.g. raw BLE manufacturer data) so
// it can be embedded as a plain JSON string value in a Report - mirrors the
// uppercase-hex convention used for RFID UIDs elsewhere in this codebase.
String hexEncode(const std::string& raw) {
    String out;
    out.reserve(raw.size() * 2);
    for (unsigned char b : raw) {
        if (b < 0x10) {
            out += '0';
        }
        out += String(b, HEX);
    }
    out.toUpperCase();
    return out;
}

}  // namespace

// NimBLEScanCallbacks subclass invoked by the BLE stack's own task for
// every advertisement seen (wantDuplicates=true in begin(), so this fires
// repeatedly for the same device, not just the first time). Keeps no state
// of its own - just copies out what it needs and hands it to
// BleScanner::enqueue(), which is the only place that touches shared state,
// under a mutex.
class BleScanner::AdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        PendingEvent event;
        event.address = String(advertisedDevice->getAddress().toString().c_str());
        event.name = advertisedDevice->haveName() ? String(advertisedDevice->getName().c_str()) : String();
        event.rssi = advertisedDevice->getRSSI();
        event.manufacturerDataHex =
            advertisedDevice->haveManufacturerData() ? hexEncode(advertisedDevice->getManufacturerData()) : String();
        BleScanner::instance().enqueue(event);
    }
};

BleScanner& BleScanner::instance() {
    static BleScanner scanner;
    return scanner;
}

void BleScanner::begin() {
    if (_began) {
        return;
    }

    _mutex = xSemaphoreCreateMutex();
    _callbacks = new AdvertisedDeviceCallbacks();

    // WifiConnection disables WiFi modem sleep entirely (WiFi.setSleep(false),
    // WIFI_PS_NONE) to fix input latency on boards with a shared WiFi/BT
    // radio - but the coexistence scheduler needs WiFi's periodic sleep
    // windows to time-share it with BT. With WIFI_PS_NONE, enabling the BT
    // controller crashes with `abort() ... coex_core_enable` on such boards
    // (confirmed via serial monitor + esp32_exception_decoder backtrace:
    // BLEDevice::init() -> btStart() -> esp_bt_controller_enable() ->
    // coex_enable() -> coex_core_enable() -> abort()). Switching to the
    // lightest modem-sleep mode restores just enough periodic sleep for
    // coexistence to work, at the cost of reintroducing a little of that
    // latency - but only on nodes that actually configure a BLEView.
    WiFi.setSleep(WIFI_PS_MIN_MODEM);

    NimBLEDevice::init("");
    NimBLEScan* scan = NimBLEDevice::getScan();
    // wantDuplicates=true so onResult() fires for every advertisement (not
    // just the first sighting of a device) - required to satisfy "forward
    // every BLE advertisement received" (see loop()'s onAdvertisement call).
    scan->setScanCallbacks(_callbacks, /*wantDuplicates=*/true);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    _began = true;

    TaskHandle_t taskHandle = nullptr;
    xTaskCreate(&BleScanner::scanTask, "BleScan", kScanTaskStackSize, this, /*priority=*/1, &taskHandle);
    _scanTaskHandle = taskHandle;
}

void BleScanner::scanTask(void* param) {
    (void)param;
    // Runs forever on its own dedicated task: NimBLEScan::getResults(ms, false)
    // blocks (this task only) until the scan completes, then we immediately
    // start another cycle - continuous scanning without ever blocking the
    // Arduino loop() task. See the class comment in BleScanner.h for why
    // this must be a separate task rather than a scan-complete callback
    // that re-arms itself from inside the BLE stack's own event task.
    NimBLEScan* scan = NimBLEDevice::getScan();
    for (;;) {
        // NimBLE's duration is milliseconds (unlike the previous Bluedroid
        // library's start(), which took seconds) - kScanCycleSeconds * 1000.
        scan->getResults(kScanCycleSeconds * 1000, false);
        scan->clearResults();
    }
}

void BleScanner::enqueue(const PendingEvent& event) {
    SemaphoreHandle_t mutex = static_cast<SemaphoreHandle_t>(_mutex);
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;  // Main loop is busy; drop this advertisement rather than block the BLE stack's task.
    }
    if (_pending.size() < kMaxPendingEvents) {
        _pending.push_back(event);
    }
    xSemaphoreGive(mutex);
}

BleDeviceInfo* BleScanner::findDevice(const String& address) {
    for (auto& device : _devices) {
        if (device.address == address) {
            return &device;
        }
    }
    return nullptr;
}

void BleScanner::loop() {
    if (!_began) {
        return;
    }

    std::vector<PendingEvent> drained;
    SemaphoreHandle_t mutex = static_cast<SemaphoreHandle_t>(_mutex);
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        drained.swap(_pending);
        xSemaphoreGive(mutex);
    }

    unsigned long now = millis();
    for (const auto& event : drained) {
        BleDeviceInfo* existing = findDevice(event.address);
        if (existing == nullptr) {
            BleDeviceInfo info{event.address, event.name, event.rssi, now};
            _devices.push_back(info);
            if (_onDiscovered) {
                _onDiscovered(info);
            }
        } else {
            existing->name = event.name;
            existing->rssi = event.rssi;
            existing->lastSeenMs = now;
        }

        if (_onAdvertisement) {
            _onAdvertisement(BleAdvertisement{event.address, event.name, event.rssi, event.manufacturerDataHex});
        }
    }

    for (size_t i = 0; i < _devices.size();) {
        if (now - _devices[i].lastSeenMs >= kDeviceTimeoutMs) {
            String address = _devices[i].address;
            _devices.erase(_devices.begin() + i);
            if (_onLost) {
                _onLost(address);
            }
        } else {
            ++i;
        }
    }
}
