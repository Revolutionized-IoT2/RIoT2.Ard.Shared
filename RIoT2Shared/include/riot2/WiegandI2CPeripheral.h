#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <riot2/GpioPeripheral.h>
#include <riot2/IPeripheral.h>

// Reads card codes from a RIoT2.Ard.WiegandI2C module (an ATtiny85 that decodes a 26-bit Wiegand
// reader and serves the last code as an I2C slave) and publishes each code as a Report for this
// device's first reportTemplate (plan M5 step 4). The module plugs into the board's Grove PORT.A:
// A1 is SDA and A2 is SCL (see GpioPinMap), so it excludes Rfid2Peripheral and GpioPeripheral
// A1/A2 slots on the same node.
//
// The module's protocol: any 3-byte read returns the 24-bit code (facility and card number, MSB
// first) and clears it; it returns 0, 0, 0 when no new code waits, and drives its IRQ pin high
// while one does. Every read code is reported, so the same card read twice is reported twice.
//
// deviceParameters:
//   - "address" (default "0x26"): the module's I2C address, decimal or 0x-hex.
//   - "irq" ("" by default, or "B1"/"B2"): the Grove pin wired to the module's IRQ output. With it,
//     the bus is only read while IRQ is high and the code 0 is reported too; without it, the module
//     is polled every kPollIntervalMs and 0 means "no new code".
//   - "format" ("hex" by default): "hex" reports six upper-case hex digits ("12AB3F"), "decimal"
//     the 24-bit number ("1223487"), "facility" the 8-bit facility code and 16-bit card number
//     ("18:43839"), as printed on many 26-bit cards.
class WiegandI2CPeripheral : public IPeripheral {
public:
    explicit WiegandI2CPeripheral(const GpioPinMap& pinMap, TwoWire& wire = Wire) : _pinMap(pinMap), _wire(wire) {}

    void begin(const DeviceConfiguration& config) override;
    void loop() override;

    // The report text for a code; an unknown format falls back to "hex".
    static String formatCode(uint32_t code, const String& format);

    static constexpr uint8_t kDefaultAddress = 0x26;
    // Without IRQ: how often the module is polled. With IRQ: how often the pin is checked.
    static constexpr unsigned long kPollIntervalMs = 100;
    static constexpr unsigned long kIrqCheckIntervalMs = 10;

private:
    GpioPinMap _pinMap;
    TwoWire& _wire;
    String _reportId;
    String _format;
    uint8_t _address = kDefaultAddress;
    int8_t _irqPin = -1;
    bool _ready = false;
    bool _answering = true;  // false after a failed read, so a missing module is logged once
    unsigned long _lastPollMs = 0;

    bool readCode(uint32_t& code);
};
