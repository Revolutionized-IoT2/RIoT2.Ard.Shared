#pragma once

#include <Arduino.h>

#include <functional>
#include <memory>
#include <vector>

#include <riot2/Command.h>
#include <riot2/DeviceConfiguration.h>
#include <riot2/IPeripheral.h>

// Owns every configured Grove-port peripheral (see IPeripheral) - the
// non-visual counterpart to ViewManager. Peripherals are configured exactly
// like Views (an entry in the orchestrator's NodeConfiguration, keyed by
// classFullName, resolved via PeripheralFactory) but never appear in the
// carousel: they're driven by loop() polling and inbound MQTT commands
// only, never by touch/encoder/button input.
class PeripheralManager {
public:
    // (Re)builds the peripheral set from a fresh NodeConfiguration. Safe to
    // call again for re-configuration pushes: replaces every peripheral
    // without requiring a reboot. isKnownElsewhere is consulted only for the
    // "unrecognized" diagnostic below - pass the project's
    // ViewFactory::instance().isRegistered() (ViewFactory is project-local,
    // since IView differs per project, so it can't be referenced directly
    // from this shared class) to avoid mislabeling View entries as
    // unrecognized peripherals.
    void rebuild(const NodeConfiguration& nodeConfiguration,
                 std::function<bool(const String&)> isKnownElsewhere = nullptr);

    void onReport(IPeripheral::ReportCallback callback) { _reportCallback = callback; }

    // Polled once per main loop() iteration.
    void loop();

    // Routes an inbound command to whichever peripheral owns a
    // commandTemplate matching commandId. Returns true if a peripheral
    // handled it (callers can try ViewManager::onCommand() too - since
    // commandTemplate ids are unique per configuration entry, exactly one
    // of the two will ever actually match).
    bool onCommand(const String& commandId, const Command& command);

private:
    struct Entry {
        DeviceConfiguration config;
        std::unique_ptr<IPeripheral> peripheral;
    };

    std::vector<Entry> _entries;
    IPeripheral::ReportCallback _reportCallback;
};
