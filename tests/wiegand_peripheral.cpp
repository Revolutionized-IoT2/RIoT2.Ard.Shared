// WiegandI2CPeripheral (plan M5 step 4) against the production WiegandI2C sketch: the fake I2C master
// below answers requestFrom() by running the sketch's requestEvent(), so both ends of the protocol are
// production code. Run by tests/test_firmware_p2.py.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

// The ATtiny85 side, with the register fakes of wiegand_frames.cpp.
#define ISR(vector) void vector()
#define _BV(bit) (1 << (bit))
enum { PB1 = 1, PB3 = 3, PB4 = 4, PCINT3 = 3, PCINT4 = 4, PCIE = 5,
       TOIE1 = 2, CS13 = 3, CS12 = 2, CS11 = 1 };
uint8_t DDRB, PORTB, PINB, PCMSK, GIMSK, TIMSK, TCCR1, TCNT1, SREG;
void cli() { SREG &= 0x7F; }
void sei() { SREG |= 0x80; }
struct SlaveWire {
    std::vector<uint8_t> sent;
    void begin(uint8_t) {}
    void onRequest(void (*)()) {}
    bool send(uint8_t value) { sent.push_back(value); return true; }
} TinyWireS;
void requestEvent();
void codeReceived();
// PRODUCTION_SKETCH
void bit(bool one) {
    PINB = one ? _BV(WGD_D0) : _BV(WGD_D1);
    PCINT0_vect();
    PINB = _BV(WGD_D0) | _BV(WGD_D1);
    PCINT0_vect();
}
uint32_t encodeWiegand26(uint32_t code) {
    auto ones = [](uint32_t value) { int count = 0; for (value &= 0xFFF; value; value >>= 1) count += value & 1; return count; };
    code &= 0xFFFFFF;
    bool evenParity = ones(code >> 12) % 2 != 0;
    bool oddParity = ones(code) % 2 == 0;
    return (evenParity ? (1u << 25) : 0) | (code << 1) | (oddParity ? 1u : 0);
}
void swipe(uint32_t code) {
    uint32_t encoded = encodeWiegand26(code);
    for (int i = 25; i >= 0; --i) bit((encoded >> i) & 1);
    TIM1_OVF_vect();
}

// The ESP32 side.
using String = std::string;
struct ReportTemplate { String id; };
struct CommandTemplate { String id; };
using ParameterList = std::vector<std::pair<String, String>>;
struct DeviceConfiguration {
    std::vector<ReportTemplate> reportTemplates;
    ParameterList deviceParameters;
};
String findParameter(const ParameterList& params, const String& key, const String& fallback = "") {
    for (const auto& param : params) if (param.first == key) return param.second;
    return fallback;
}
struct Command {};
struct Report { String id, value; };
std::vector<Report> published;
class IPeripheral {
public:
    virtual ~IPeripheral() = default;
    virtual void begin(const DeviceConfiguration&) = 0;
    virtual void loop() {}
    virtual void onCommand(const Command&) {}
protected:
    void publishReport(const Report& report) { published.push_back(report); }
};
struct Log {
    unsigned lines = 0;
    template<typename... Args> void printf(const char*, Args...) { ++lines; }
} Serial;
enum { LOW = 0, HIGH = 1, INPUT = 2 };
unsigned long hostMillis = 0;
unsigned long millis() { return hostMillis; }
int irqPin = -1, irqMode = -1;
void pinMode(int pin, int mode) { irqPin = pin; irqMode = mode; }
int digitalRead(int pin) { return pin == irqPin && (PORTB & _BV(WGD_IRQ)) ? HIGH : LOW; }
struct TwoWire {
    int sda = -1, scl = -1;
    bool present = true;
    bool shortRead = false;
    unsigned requests = 0;
    std::vector<uint8_t> rx;
    size_t at = 0;
    bool begin(int dataPin, int clockPin) { sda = dataPin; scl = clockPin; return true; }
    uint8_t requestFrom(uint8_t address, uint8_t size) {
        ++requests;
        rx.clear();
        at = 0;
        if (!present || address != 0x26) return 0;
        TinyWireS.sent.clear();
        requestEvent();
        rx = TinyWireS.sent;
        if (shortRead) rx.resize(2);
        if (rx.size() > size) rx.resize(size);
        return static_cast<uint8_t>(rx.size());
    }
    int available() { return static_cast<int>(rx.size() - at); }
    int read() { return at < rx.size() ? rx[at++] : -1; }
} Wire;
// PRODUCTION_PIN_MAP
// PRODUCTION_CLASS
// PRODUCTION_METHODS
// PRODUCTION_MAPS

void run(WiegandI2CPeripheral& peripheral, unsigned long ms) {
    for (unsigned long end = hostMillis + ms; hostMillis < end; ++hostMillis) peripheral.loop();
}

DeviceConfiguration configuration(ParameterList parameters) {
    DeviceConfiguration config;
    config.reportTemplates = {{"card"}};
    config.deviceParameters = parameters;
    return config;
}

int main() {
    assert(WiegandI2CPeripheral::formatCode(0x12AB3F, "hex") == "12AB3F");
    assert(WiegandI2CPeripheral::formatCode(42, "hex") == "00002A");
    assert(WiegandI2CPeripheral::formatCode(0x12AB3F, "decimal") == "1223487");
    assert(WiegandI2CPeripheral::formatCode(0x12AB3F, "facility") == "18:43839");
    assert(WiegandI2CPeripheral::formatCode(0xFF12AB3F, "unknown") == "12AB3F");
    setup();

    // Core2, polled: 0, 0, 0 is "no new code"; every swipe is reported, the same card twice too.
    WiegandI2CPeripheral core(kM5Core2GroveMap);
    core.begin(configuration({}));
    assert(Wire.sda == 32 && Wire.scl == 33 && Serial.lines == 0);
    run(core, 1000);
    assert(published.empty() && Wire.requests == 9);
    swipe(0x12AB3F);
    run(core, 100);
    assert(published.size() == 1 && published[0].id == "card" && published[0].value == "\"12AB3F\"");
    swipe(0x12AB3F);
    run(core, 100);
    assert(published.size() == 2 && published[1].value == "\"12AB3F\"");
    run(core, 500);
    assert(published.size() == 2);

    // A missing module is logged once, and once more when it answers again; a short read is a failure.
    Wire.present = false;
    run(core, 1000);
    assert(Serial.lines == 1);
    Wire.present = true;
    Wire.shortRead = true;
    swipe(0x000001);
    run(core, 100);
    assert(Serial.lines == 1 && published.size() == 2);
    Wire.shortRead = false;
    swipe(0x000002);
    run(core, 100);
    assert(Serial.lines == 2 && published.size() == 3 && published[2].value == "\"000002\"");

    // Dial with IRQ on B2: the bus is read only while IRQ is high, and the code 0 is a card.
    published.clear();
    WiegandI2CPeripheral dial(kM5DialGroveMap);
    dial.begin(configuration({{"irq", "B2"}, {"format", "facility"}, {"address", "38"}}));
    assert(Wire.sda == 13 && Wire.scl == 15 && irqPin == 1 && irqMode == INPUT && Serial.lines == 2);
    unsigned requests = Wire.requests;
    run(dial, 1000);
    assert(Wire.requests == requests && published.empty());
    swipe(0);
    run(dial, 20);
    assert(published.size() == 1 && published[0].value == "\"0:0\"" && Wire.requests == requests + 1);
    assert(!(PORTB & _BV(WGD_IRQ)));
    swipe(0x12AB3F);
    run(dial, 20);
    assert(published.size() == 2 && published[1].value == "\"18:43839\"");

    // Bad parameters fall back with a warning: address 0x26, polling instead of an A-port IRQ.
    published.clear();
    WiegandI2CPeripheral fallback(kM5DialGroveMap);
    fallback.begin(configuration({{"address", "0x80"}, {"irq", "A1"}, {"format", "decimal"}}));
    assert(Serial.lines == 4);
    swipe(0x12AB3F);
    run(fallback, 200);
    assert(published.size() == 1 && published[0].value == "\"1223487\"");

    // Without a reportTemplate nothing is read.
    WiegandI2CPeripheral silent(kM5Core2GroveMap);
    silent.begin(DeviceConfiguration{});
    requests = Wire.requests;
    run(silent, 1000);
    assert(Wire.requests == requests && Serial.lines == 5);
}
