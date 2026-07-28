#pragma once

#include <Arduino.h>

// Lightweight POD types shared between BleScanner (the node's on-device BLE
// scan wrapper, see BleScanner.h) and IView/BLEView - kept in their own
// header (rather than folded into BleScanner.h) so IView.h doesn't need to
// pull in the BLE scanning implementation just to declare its
// onBleXxx() hooks.

// A nearby BLE advertiser currently considered "present" (seen within the
// last BleScanner::kDeviceTimeoutMs).
struct BleDeviceInfo {
    String address;           // e.g. "aa:bb:cc:dd:ee:ff"
    String name;               // advertised name, empty if not present
    int rssi = 0;
    unsigned long lastSeenMs = 0;
};

// A single BLE advertisement as it arrives off the air - fired for every
// advertisement received (not just the first time a device is seen).
struct BleAdvertisement {
    String address;
    String name;
    int rssi = 0;
    String manufacturerDataHex;  // hex-encoded raw manufacturer data, empty if none
};
