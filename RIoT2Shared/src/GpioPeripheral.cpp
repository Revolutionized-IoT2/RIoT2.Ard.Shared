#include <riot2/GpioPeripheral.h>

GpioPeripheral::Slot* GpioPeripheral::findOrCreateSlot(const String& address) {
    for (auto& slot : _slots) {
        if (slot.address == address) {
            return &slot;
        }
    }
    Slot slot;
    slot.address = address;
    _slots.push_back(slot);
    return &_slots.back();
}

void GpioPeripheral::begin(const DeviceConfiguration& config) {
    _slots.clear();
    _pullup = findParameter(config.deviceParameters, "pullup", "true") == "true";
    _invert = findParameter(config.deviceParameters, "invert", "false") == "true";

    for (const auto& cmd : config.commandTemplates) {
        if (cmd.address.length() == 0) continue;
        Slot* slot = findOrCreateSlot(cmd.address);
        slot->command = cmd;
        slot->hasCommand = true;
        slot->isOutput = true;
    }

    for (const auto& report : config.reportTemplates) {
        if (report.address.length() == 0) continue;
        Slot* slot = findOrCreateSlot(report.address);
        slot->report = report;
        slot->hasReport = true;
    }

    for (auto& slot : _slots) {
        slot.pin = _pinMap.resolve(slot.address);
        if (slot.pin < 0) {
            Serial.printf("[GpioPeripheral] Unrecognized Grove address \"%s\", ignoring\n", slot.address.c_str());
            continue;
        }

        if (slot.isOutput) {
            if (!_pinMap.canOutput(slot.address)) {
                Serial.printf("[GpioPeripheral] Grove address \"%s\" is input-only, ignoring output\n",
                              slot.address.c_str());
                slot.pin = -1;
                continue;
            }
            pinMode(slot.pin, OUTPUT);
            slot.lastState = false;
            digitalWrite(slot.pin, _invert ? HIGH : LOW);
        } else {
            pinMode(slot.pin, _pullup ? INPUT_PULLUP : INPUT);
            bool raw = digitalRead(slot.pin) == HIGH;
            slot.lastState = _invert ? !raw : raw;
            slot.pendingState = slot.lastState;
            slot.pendingSinceMs = millis();
        }
    }
}

void GpioPeripheral::loop() {
    unsigned long now = millis();
    for (auto& slot : _slots) {
        if (slot.pin < 0 || slot.isOutput || !slot.hasReport) continue;

        bool raw = digitalRead(slot.pin) == HIGH;
        bool value = _invert ? !raw : raw;

        if (value != slot.pendingState) {
            slot.pendingState = value;
            slot.pendingSinceMs = now;
        }

        if (value != slot.lastState && (now - slot.pendingSinceMs) >= kDebounceMs) {
            slot.lastState = value;
            publishReport(Report{slot.report.id, value ? "true" : "false"});
        }
    }
}

void GpioPeripheral::onCommand(const Command& command) {
    for (auto& slot : _slots) {
        if (slot.pin < 0 || !slot.isOutput || !slot.hasCommand || slot.command.id != command.id) continue;

        bool value = command.value.is<bool>() ? command.value.as<bool>() : command.value.as<int>() != 0;
        slot.lastState = value;
        digitalWrite(slot.pin, (_invert ? !value : value) ? HIGH : LOW);
        return;
    }
}
