#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct String : std::string {
    using std::string::string;
    String() = default;
    String(const std::string& value) : std::string(value) {}
    explicit String(int value) : std::string(std::to_string(value)) {}
    long toInt() const { return std::strtol(c_str(), nullptr, 10); }
    bool equalsIgnoreCase(const String& other) const {
        if (size() != other.size()) return false;
        for (size_t i = 0; i < size(); ++i) {
            if (std::tolower((*this)[i]) != std::tolower(other[i])) return false;
        }
        return true;
    }
};
unsigned long nowMs = 0;
unsigned long millis() { return nowMs; }
struct M5Canvas {
    int width() const { return 240; }
    int height() const { return 240; }
    template<typename... T> void fillScreen(T...) {}
    template<typename... T> void fillArc(T...) {}
    template<typename... T> void setTextColor(T...) {}
    template<typename... T> void setTextDatum(T...) {}
    template<typename... T> void setTextSize(T...) {}
    template<typename... T> void drawString(T...) {}
};
constexpr int BLACK = 0, WHITE = 1, middle_center = 0;
constexpr uint16_t kAccentColor = 1, kTrackColor = 2;
// TIMER_CONSTANTS
struct Command {
    String id;
    struct Value {
        int number;
        template<typename T> T as() const { return static_cast<T>(number); }
    } value;
};
// PRODUCTION_CONFIG
// PRODUCTION_BLE_TYPES
// PRODUCTION_REPORT
// PRODUCTION_IVIEW
#define private public
// PRODUCTION_TIMER
#undef private
namespace Buzzer {
unsigned confirms = 0, taps = 0, rings = 0;
void confirm() { ++confirms; }
void tap() { ++taps; }
void ring() { ++rings; }
}
// TIMER_METHODS
struct Log {
    template<typename... T> void printf(const char*, T...) {}
} Serial;
class ClockView : public IView {
public:
    void begin(const DeviceConfiguration&) override {}
    void render(M5Canvas&) override {}
};
class PopupView : public ClockView {
public:
    bool isAlert() const override { return true; }
};
class ViewFactory {
public:
    static ViewFactory& instance() { static ViewFactory f; return f; }
    std::unique_ptr<IView> create(const String& name) {
        if (name == "TimerView") return std::make_unique<TimerView>();
        if (name == "PopupView") return std::make_unique<PopupView>();
        return nullptr;
    }
};
struct PeripheralFactory {
    static PeripheralFactory& instance() { static PeripheralFactory f; return f; }
    bool isRegistered(const String&) { return false; }
};
struct BleScanner {
    static BleScanner& instance() { static BleScanner scanner; return scanner; }
    std::vector<BleDeviceInfo> snapshot() { return {}; }
};
#define private public
// PRODUCTION_MANAGER
#undef private
// MANAGER_METHODS

NodeConfiguration config(bool beep = false) {
    DeviceConfiguration timer;
    timer.name = "Timer";
    timer.classFullName = "TimerView";
    timer.deviceParameters = {{"defaultMinutes", "1"}, {"stepMinutes", "1"},
                              {"maxMinutes", "5"}, {"beepOnComplete", beep ? "true" : "false"}};
    CommandTemplate command;
    command.id = "timer-command";
    timer.commandTemplates.push_back(command);
    ReportTemplate report;
    report.id = "timer-report";
    timer.reportTemplates.push_back(report);
    DeviceConfiguration popup;
    popup.classFullName = "PopupView";
    command.id = "popup-command";
    popup.commandTemplates.push_back(command);
    NodeConfiguration node;
    node.deviceConfigurations = {timer, popup};
    return node;
}
TimerView& timer(ViewManager& manager) {
    return *static_cast<TimerView*>(manager._entries[0].view.get());
}
int main() {
    std::vector<Report> reports;
    ViewManager manager;
    manager.onReport([&](const Report& report) { reports.push_back(report); });
    auto node = config();
    manager.rebuild(node);
    manager.enterFocused(0);
    timer(manager).onTouch(0, 0);
    manager.exitToCarousel(); // Hidden timer, no rendering whatsoever.
    nowMs = 59999;
    manager.loop();
    assert(reports.empty() && timer(manager).keepsAwake());
    nowMs = 60000;
    manager.loop();
    assert(reports.size() == 1 && reports[0].id == "timer-report" && reports[0].value == "0");
    assert(!timer(manager).keepsAwake() && timer(manager)._phase == TimerView::Phase::Done);
    manager.onCommand("timer-command", {"timer-command", {5}});
    assert(timer(manager)._minutes == 1); // Done also ignores preset commands.
    for (unsigned i = 0; i < 20; ++i) manager.loop();
    assert(reports.size() == 1); // Completion exactly once, including after later rendering.
    M5Canvas canvas;
    timer(manager).render(canvas);
    assert(reports.size() == 1);

    manager.rebuild(node);
    manager.enterFocused(0);
    timer(manager).onTouch(0, 0);
    assert(manager.onCommand("popup-command", {"popup-command", {0}}));
    assert(manager.activeView()->isAlert());
    nowMs += 60000;
    manager.loop();
    assert(reports.size() == 2 && manager.activeView()->isAlert()); // No forced navigation.

    manager.rebuild(node);
    manager.onCommand("timer-command", {"timer-command", {0}});
    assert(timer(manager)._minutes == 1);
    manager.onCommand("timer-command", {"timer-command", {99}});
    assert(timer(manager)._minutes == 5);
    manager.onCommand("timer-command", {"timer-command", {1}});
    timer(manager).onTouch(0, 0);
    manager.onCommand("timer-command", {"timer-command", {5}});
    assert(timer(manager)._minutes == 1); // Commands still cannot reset a running countdown.
    timer(manager).onTouch(0, 0); // Cancel.
    nowMs += 60000;
    manager.loop();
    assert(reports.size() == 2 && timer(manager).isInteracting());

    timer(manager).onTouch(0, 0);
    nowMs += 60000;
    unsigned sounds = Buzzer::confirms;
    timer(manager).render(canvas);
    assert(reports.size() == 2 && Buzzer::confirms == sounds); // Rendering has no execution side effects.
    manager.loop(); // Diagnostics/idle also use this path without calling any view render().
    assert(reports.size() == 3 && Buzzer::confirms == sounds + 1);

    manager.rebuild(node);
    timer(manager).onTouch(0, 0);
    nowMs += 30000;
    manager.rebuild(node); // Destroys an in-progress timer; new view starts in Setting.
    nowMs += 60000;
    manager.loop();
    assert(reports.size() == 3 && timer(manager).isInteracting());
    timer(manager).onTouch(0, 0);
    manager.rebuild(NodeConfiguration{}); // Removing the timer owns no delayed callback.
    nowMs += 60000;
    manager.loop();
    assert(reports.size() == 3 && !manager.hasViews());

    manager.rebuild(config(true));
    nowMs = std::numeric_limits<unsigned long>::max() - 60099;
    timer(manager).onTouch(0, 0);
    nowMs += 60000; // First ring just before millis rollover.
    manager.loop();
    assert(reports.size() == 4 && Buzzer::rings == 1);
    timer(manager).render(canvas);
    assert(Buzzer::rings == 1);
    for (unsigned i = 1; i < 6; ++i) {
        nowMs += 259;
        manager.loop();
        assert(Buzzer::rings == i);
        ++nowMs;
        manager.loop();
        assert(Buzzer::rings == i + 1);
    }
    nowMs += 10000;
    manager.loop();
    assert(reports.size() == 4 && Buzzer::rings == 6);
    timer(manager).onTouch(0, 0); // Dismiss then restart.
    timer(manager).onTouch(0, 0);
    nowMs += 60000;
    manager.loop();
    assert(reports.size() == 5 && Buzzer::rings == 7);
    manager.rebuild(node); // Cancels a partially played ring sequence.
    nowMs += 10000;
    manager.loop();
    assert(Buzzer::rings == 7 && reports.size() == 5);

    manager.rebuild(config(true));
    timer(manager).onTouch(0, 0);
    nowMs += 60000;
    manager.loop();
    assert(Buzzer::rings == 8);
    timer(manager).begin(node.deviceConfigurations[0]); // Explicit reinit stops ringing too.
    nowMs += 10000;
    manager.loop();
    assert(Buzzer::rings == 8 && reports.size() == 6);

    timer(manager).begin(config(true).deviceConfigurations[0]);
    timer(manager).onTouch(0, 0);
    nowMs += 60000;
    manager.loop();
    assert(reports.size() == 7 && Buzzer::rings == 9);
    timer(manager).onTouch(0, 0); // Dismiss a completed timer.
    nowMs += 10000;
    manager.loop();
    assert(reports.size() == 7 && Buzzer::rings == 9);

    auto twoTimers = node;
    auto second = node.deviceConfigurations[0];
    second.reportTemplates[0].id = "second-timer-report";
    twoTimers.deviceConfigurations.push_back(second);
    manager.rebuild(twoTimers);
    nowMs = std::numeric_limits<unsigned long>::max() - 30000;
    timer(manager).onTouch(0, 0);
    manager._entries[2].view->onTouch(0, 0);
    manager._idle = true; // Neither visibility nor the idle state gates background work.
    nowMs += 59999; // Countdown straddles millis rollover.
    manager.loop();
    assert(reports.size() == 7);
    ++nowMs;
    manager.loop();
    assert(reports.size() == 9 && reports[8].id == "second-timer-report");
    manager.loop();
    assert(reports.size() == 9);
}
