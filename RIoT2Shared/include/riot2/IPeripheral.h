#pragma once

#include <Arduino.h>

#include <functional>

#include <riot2/Command.h>
#include <riot2/DeviceConfiguration.h>
#include <riot2/Report.h>

// Common interface for a Grove-port peripheral driver - the non-visual
// counterpart to IView. Peripherals are configured the same way as Views
// (an entry in the orchestrator's configuration keyed by classFullName,
// with commandTemplates/reportTemplates/deviceParameters) but never appear
// in the carousel and have no rendering/touch/encoder hooks: they just
// drive physical pins on the board's Grove ports (PORT.A / PORT.B) and
// turn inbound Commands / physical pin changes into MQTT Commands/Reports,
// exactly like a View does for its on-screen controls.
class IPeripheral {
public:
    using ReportCallback = std::function<void(const Report&)>;

    virtual ~IPeripheral() = default;

    // One-time setup from the orchestrator-provided configuration for this
    // peripheral (see PeripheralManager::rebuild()).
    virtual void begin(const DeviceConfiguration& config) = 0;

    // Polled once per main loop() iteration regardless of what the carousel
    // is currently showing (peripherals have no UI/focus concept) - use
    // this for polling physical pins/buses and publishing reports on change.
    virtual void loop() {}

    // Apply an inbound command addressed to one of this peripheral's
    // commandTemplates.
    virtual void onCommand(const Command& command) { (void)command; }

    // Set by PeripheralManager before begin(). Peripherals call
    // publishReport() to emit a Report for one of their reportTemplates.
    void setReportCallback(ReportCallback callback) { _reportCallback = callback; }

protected:
    void publishReport(const Report& report) {
        if (_reportCallback) {
            _reportCallback(report);
        }
    }

private:
    ReportCallback _reportCallback;
};
