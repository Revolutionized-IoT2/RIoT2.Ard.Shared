#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using String = std::string;
struct CommandTemplate { String id, address; };
struct ReportTemplate { String id, address; };
struct DeviceConfiguration {
    std::vector<CommandTemplate> commandTemplates;
    std::vector<ReportTemplate> reportTemplates;
    int deviceParameters = 0;
};
String findParameter(int, const char*, const char* fallback) { return fallback; }
struct Value {
    bool state = false;
    template<typename T> bool is() const { return true; }
    template<typename T> T as() const { return static_cast<T>(state); }
};
struct Command { String id; Value value; };
struct Report { String id, value; };
class IPeripheral {
public:
    virtual ~IPeripheral() = default;
    virtual void begin(const DeviceConfiguration&) = 0;
    virtual void loop() {}
    virtual void onCommand(const Command&) {}
    void publishReport(const Report&) {}
};
struct Log {
    unsigned warnings = 0;
    template<typename... Args> void printf(const char*, Args...) { ++warnings; }
} Serial;
enum { OUTPUT = 1, INPUT = 2, INPUT_PULLUP = 3, LOW = 0, HIGH = 1 };
struct PinCall { int pin, value; };
std::vector<PinCall> modes, writes;
void pinMode(int pin, int mode) { modes.push_back({pin, mode}); }
void digitalWrite(int pin, int state) { writes.push_back({pin, state}); }
int digitalRead(int) { return HIGH; }
unsigned long millis() { return 100; }
// PRODUCTION_CLASS
// PRODUCTION_METHODS
// PRODUCTION_MAPS
int main() {
    assert(kM5Core2GroveMap.canOutput("A1"));
    assert(kM5Core2GroveMap.canOutput("A2"));
    assert(kM5Core2GroveMap.canOutput("B1"));
    assert(!kM5Core2GroveMap.canOutput("B2"));
    assert(!kM5Core2GroveMap.canOutput("unknown"));
    assert(kM5DialGroveMap.canOutput("B2"));
    DeviceConfiguration config;
    config.commandTemplates = {{"bad", "B2"}, {"relay", "B1"}};
    GpioPeripheral core(kM5Core2GroveMap);
    core.begin(config);
    assert(Serial.warnings == 1 && modes.size() == 1 && writes.size() == 1);
    assert(modes[0].pin == 26 && modes[0].value == OUTPUT);
    core.onCommand({"bad", {true}});
    assert(writes.size() == 1);
    core.onCommand({"relay", {true}});
    assert(writes.size() == 2 && writes.back().pin == 26 && writes.back().value == HIGH);
    modes.clear();
    writes.clear();
    config.commandTemplates.clear();
    config.reportTemplates = {{"input", "B2"}};
    core.begin(config);
    assert(modes.size() == 1 && modes[0].pin == 36 && writes.empty());
    modes.clear();
    config.reportTemplates.clear();
    config.commandTemplates = {{"dial-output", "B2"}};
    GpioPeripheral dial(kM5DialGroveMap);
    dial.begin(config);
    dial.onCommand({"dial-output", {true}});
    assert(modes.size() == 1 && modes[0].pin == 1 && writes.back().value == HIGH);
}
