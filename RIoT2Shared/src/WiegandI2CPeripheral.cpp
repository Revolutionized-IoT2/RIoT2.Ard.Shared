#include <riot2/WiegandI2CPeripheral.h>

#include <stdlib.h>

void WiegandI2CPeripheral::begin(const DeviceConfiguration& config) {
    _reportId = config.reportTemplates.empty() ? String() : config.reportTemplates.front().id;
    _format = findParameter(config.deviceParameters, "format", "hex");
    _ready = false;
    _answering = true;
    _lastPollMs = millis();

    String address = findParameter(config.deviceParameters, "address", "0x26");
    char* end = nullptr;
    long parsed = strtol(address.c_str(), &end, 0);
    if (address.length() == 0 || end == nullptr || *end != '\0' || parsed < 0x08 || parsed > 0x77) {
        Serial.printf("[WiegandI2C] Invalid I2C address \"%s\", using 0x%02X\n", address.c_str(), kDefaultAddress);
        parsed = kDefaultAddress;
    }
    _address = static_cast<uint8_t>(parsed);

    _irqPin = -1;
    String irq = findParameter(config.deviceParameters, "irq", "");
    if (irq.length() > 0) {
        if (irq == "B1" || irq == "B2") _irqPin = _pinMap.resolve(irq);
        if (_irqPin < 0) {
            Serial.printf("[WiegandI2C] IRQ pin \"%s\" isn't B1 or B2 on this board, polling instead\n", irq.c_str());
        } else {
            pinMode(_irqPin, INPUT);  // The module drives IRQ push-pull.
        }
    }

    if (_reportId.length() == 0) {
        Serial.printf("[WiegandI2C] No reportTemplate, nothing to publish\n");
        return;
    }
    if (_pinMap.a1 < 0 || _pinMap.a2 < 0) {
        Serial.printf("[WiegandI2C] This board has no Grove PORT.A\n");
        return;
    }
    _wire.begin(_pinMap.a1, _pinMap.a2);
    _ready = true;
}

void WiegandI2CPeripheral::loop() {
    if (!_ready) return;
    unsigned long now = millis();
    if (now - _lastPollMs < (_irqPin >= 0 ? kIrqCheckIntervalMs : kPollIntervalMs)) return;
    _lastPollMs = now;
    if (_irqPin >= 0 && digitalRead(_irqPin) != HIGH) return;

    uint32_t code = 0;
    if (!readCode(code)) return;
    // Without IRQ, 0, 0, 0 is the module's "no new code".
    if (code == 0 && _irqPin < 0) return;
    publishReport(Report{_reportId, String("\"") + formatCode(code, _format) + "\""});
}

bool WiegandI2CPeripheral::readCode(uint32_t& code) {
    uint8_t received = _wire.requestFrom(_address, static_cast<uint8_t>(3));
    if (received != 3) {
        while (_wire.available()) _wire.read();
        if (_answering) Serial.printf("[WiegandI2C] No answer from the module at 0x%02X\n", _address);
        _answering = false;
        return false;
    }
    code = 0;
    for (int i = 0; i < 3; ++i) code = (code << 8) | static_cast<uint8_t>(_wire.read());
    if (!_answering) Serial.printf("[WiegandI2C] The module at 0x%02X answers again\n", _address);
    _answering = true;
    return true;
}

String WiegandI2CPeripheral::formatCode(uint32_t code, const String& format) {
    char text[16];
    code &= 0xFFFFFFUL;
    if (format == "decimal") {
        snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(code));
    } else if (format == "facility") {
        snprintf(text, sizeof(text), "%u:%u", static_cast<unsigned>(code >> 16), static_cast<unsigned>(code & 0xFFFFUL));
    } else {
        snprintf(text, sizeof(text), "%06lX", static_cast<unsigned long>(code));
    }
    return String(text);
}
