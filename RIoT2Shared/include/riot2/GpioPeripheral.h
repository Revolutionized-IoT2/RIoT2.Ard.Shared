#pragma once

#include <Arduino.h>

#include <vector>

#include <riot2/IPeripheral.h>

// Drives up to 4 raw digital GPIO pins broken out on a board's Grove
// (HY2.0-4P) ports. Each pin is addressed as "A1"/"A2" (PORT.A) or "B1"/"B2"
// (PORT.B) via a commandTemplate/reportTemplate's `address` field - the same
// address-based slot-correlation convention ButtonView/ToggleView use (see
// DeviceConfiguration.h). A pin is configured as:
//   - OUTPUT, if any commandTemplate references its address: an inbound
//     Command sets the pin HIGH/LOW (bool value).
//   - INPUT (INPUT_PULLUP by default), if only a reportTemplate references
//     its address: the pin is polled and debounced in loop(), publishing a
//     Report on every confirmed state change.
// deviceParameters:
//   - "pullup" ("true"/"false", default "true") - whether input pins use
//     their internal pull-up resistor.
//   - "invert" ("true"/"false", default "false") - flips the reported/
//     applied boolean sense for every pin on this peripheral.
//
// The A1/A2/B1/B2 -> physical GPIO pin mapping is board-specific and is
// supplied by the caller via GpioPinMap (each project defines its own Grove
// pin table and passes it in when registering this peripheral in its own
// main.cpp), rather than hardcoded here.
struct GpioPinMap {
    int8_t a1 = -1;
    int8_t a2 = -1;
    int8_t b1 = -1;
    int8_t b2 = -1;

    int8_t resolve(const String& address) const {
        if (address == "A1") return a1;
        if (address == "A2") return a2;
        if (address == "B1") return b1;
        if (address == "B2") return b2;
        return -1;
    }
};

class GpioPeripheral : public IPeripheral {
public:
    explicit GpioPeripheral(const GpioPinMap& pinMap) : _pinMap(pinMap) {}

    void begin(const DeviceConfiguration& config) override;
    void loop() override;
    void onCommand(const Command& command) override;

private:
    struct Slot {
        String address;
        int8_t pin = -1;
        bool isOutput = false;
        bool hasReport = false;
        ReportTemplate report;
        bool hasCommand = false;
        CommandTemplate command;
        bool lastState = false;
        bool pendingState = false;
        unsigned long pendingSinceMs = 0;
    };

    // Debounce window for input-pin state changes before a Report is
    // published, matching the settle time used elsewhere in this codebase
    // for mechanical switches/buttons wired to Grove ports.
    static constexpr unsigned long kDebounceMs = 30;

    GpioPinMap _pinMap;
    std::vector<Slot> _slots;
    bool _pullup = true;
    bool _invert = false;

    Slot* findOrCreateSlot(const String& address);
};
