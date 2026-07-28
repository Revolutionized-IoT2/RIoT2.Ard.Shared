#pragma once

#include <Arduino.h>

#include <functional>
#include <vector>

#include <riot2/BleTypes.h>

// Wraps the board's on-chip BLE radio (via NimBLE-Arduino - NimBLEDevice/
// NimBLEScan/NimBLEScanCallbacks) to continuously discover nearby BLE
// advertisers in the background. NimBLE is used instead of the ESP32
// Arduino core's bundled Bluedroid BLE library because its host stack uses
// substantially less RAM - on PSRAM-less modules, WiFi + a Bluedroid BLE
// host together left only ~11-24KB of free heap, too little for reliable
// display rendering. Like an on-device RFID reader, the BLE radio is only
// powered on if the active configuration actually includes a view that
// wants BLE data (BLEView, see ViewManager::hasBleConsumer()) - it stays off
// otherwise, rather than being enabled unconditionally at boot.
//
// The underlying BLE stack invokes its scan-result callback from its own
// FreeRTOS task, not the Arduino loop() task, so results are handed off
// through a small mutex-guarded pending queue and only turned into
// onDeviceDiscovered()/onDeviceLost()/onAdvertisement() callbacks (and
// _devices list updates) from inside loop() - keeping all app-visible state
// single-threaded, consistent with the rest of this codebase (MQTT
// publish, view rendering, etc. all happen on the main loop() task).
//
// Continuous scanning is implemented with a dedicated low-priority
// background FreeRTOS task (see scanTask()) that repeatedly performs a
// *blocking* NimBLEScan::getResults(durationMs, false) call - blocking only
// that background task, never the Arduino loop() task. This deliberately
// avoids the alternative "non-blocking start() + onScanEnd that re-arms
// itself" pattern: that callback fires from inside the BLE stack's own
// event-processing task, and calling back into the scan-start APIs from
// there caused reproducible device resets shortly after the scanner
// started (confirmed with the original Bluedroid-based implementation) -
// recursing into the BLE stack from its own event-dispatch context is not
// a supported pattern (no bundled BLE example does this; they all call
// start()/getResults() from a separate task/loop instead).
class BleScanner {
public:
    using DeviceEventCallback = std::function<void(const BleDeviceInfo&)>;
    using DeviceLostCallback = std::function<void(const String& address)>;
    using AdvertisementCallback = std::function<void(const BleAdvertisement&)>;

    static BleScanner& instance();

    // Initializes the BLE stack and starts continuous background scanning.
    // Safe to call more than once (no-op after the first call).
    void begin();

    // Call once per loop() iteration while BLE is active: drains scan
    // events queued by the BLE stack's own task and prunes devices not
    // seen for kDeviceTimeoutMs (firing onDeviceLost() for each).
    void loop();

    bool isActive() const { return _began; }

    void onDeviceDiscovered(DeviceEventCallback callback) { _onDiscovered = std::move(callback); }
    void onDeviceLost(DeviceLostCallback callback) { _onLost = std::move(callback); }
    void onAdvertisement(AdvertisementCallback callback) { _onAdvertisement = std::move(callback); }

    // "No longer present" after this many milliseconds without a new
    // advertisement from a given address.
    static constexpr unsigned long kDeviceTimeoutMs = 60000;

private:
    // Each cycle blocks the dedicated scan task (see scanTask()) for this
    // many seconds - never the Arduino loop() task, so this can be as long
    // as desired without affecting UI/MQTT responsiveness. NimBLE's
    // getResults() takes milliseconds (unlike the previous Bluedroid
    // library's start(), which took seconds) - converted at the call site.
    static constexpr uint32_t kScanCycleSeconds = 30;
    static constexpr size_t kMaxPendingEvents = 64;
    static constexpr uint32_t kScanTaskStackSize = 4096;

    struct PendingEvent {
        String address;
        String name;
        int rssi = 0;
        String manufacturerDataHex;
    };

    class AdvertisedDeviceCallbacks;

    BleScanner() = default;

    void enqueue(const PendingEvent& event);
    BleDeviceInfo* findDevice(const String& address);

    // Body of the dedicated background scan task created by begin() (see
    // BleScanner.cpp for why this can't just be a scan-complete callback
    // that re-arms itself from inside the BLE stack's own event task).
    static void scanTask(void* param);

    bool _began = false;
    void* _mutex = nullptr;        // SemaphoreHandle_t, opaque here to avoid pulling FreeRTOS headers into this .h
    void* _scanTaskHandle = nullptr;  // TaskHandle_t, same reasoning
    std::vector<PendingEvent> _pending;
    std::vector<BleDeviceInfo> _devices;

    DeviceEventCallback _onDiscovered;
    DeviceLostCallback _onLost;
    AdvertisementCallback _onAdvertisement;

    AdvertisedDeviceCallbacks* _callbacks = nullptr;
};
