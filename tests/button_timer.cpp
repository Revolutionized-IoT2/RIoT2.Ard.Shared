#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct String : std::string {
    using std::string::string;
};
struct ReportTemplate { String id, name, address; };
struct DeviceConfiguration {
    std::vector<ReportTemplate> reportTemplates;
};
struct Command {};
struct Report { String id, value; };
namespace riot2 {
namespace views {
// Stands in for the shared model (tested in tests/views/ButtonModel_test.cpp): one button per
// report template, pressing reports "true".
class ButtonModel {
public:
    static constexpr unsigned long kFlashMs = 200;
    std::vector<ReportTemplate> reports;
    std::vector<bool> active;
    void begin(const DeviceConfiguration& config) {
        reports = config.reportTemplates;
        active.assign(reports.size(), false);
    }
    size_t size() const { return reports.size(); }
    bool isActive(size_t index) const { return active[index]; }
    bool press(size_t index, Report& report) {
        if (index >= reports.size()) {
            return false;
        }
        report = Report{reports[index].id, "true"};
        return true;
    }
};
}  // namespace views
}  // namespace riot2
struct lv_obj_t {};
struct lv_event_t { void* data; };
struct lv_timer_t {
    void (*callback)(lv_timer_t*);
    void* data;
    bool live = true;
};
std::vector<std::unique_ptr<lv_timer_t>> timers;
unsigned reports = 0, visualUpdates = 0;
bool visualState = false;
constexpr uint32_t LV_BUTTONMATRIX_BUTTON_NONE = UINT32_MAX;
uint32_t selectedButton = 0;
void* lv_event_get_user_data(lv_event_t* e) { return e->data; }
uint32_t lv_buttonmatrix_get_selected_button(lv_obj_t*) { return selectedButton; }
lv_timer_t* lv_timer_create(void (*cb)(lv_timer_t*), uint32_t period, void* data) {
    assert(period == riot2::views::ButtonModel::kFlashMs);
    timers.emplace_back(new lv_timer_t{cb, data, true});
    return timers.back().get();
}
void lv_timer_set_repeat_count(lv_timer_t*, int count) { assert(count == 1); }
void lv_timer_delete(lv_timer_t* t) { assert(t->live); t->live = false; }
void* lv_timer_get_user_data(lv_timer_t* t) { return t->data; }
void expireTimers() {
    for (auto& t : timers) {
        if (t->live) {
            t->callback(t.get());
            lv_timer_delete(t.get()); // LVGL one-shot auto-deletion.
        }
    }
}
size_t liveTimers() {
    return std::count_if(timers.begin(), timers.end(),
                         [](const std::unique_ptr<lv_timer_t>& t) { return t->live; });
}
class IView {
public:
    virtual ~IView() = default;
    virtual void begin(const DeviceConfiguration&) = 0;
    virtual void buildUi(lv_obj_t*) = 0;
    virtual void onCommand(const Command&) = 0;
protected:
    void publishReport(const Report& report) {
        assert(report.id == "report" && report.value == "true");
        ++reports;
    }
};

// Expose callbacks to the harness without changing their production declarations.
#define private public
// PRODUCTION_CLASS
#undef private
// PRODUCTION_METHODS

void ButtonView::buildUi(lv_obj_t*) {}
void ButtonView::onCommand(const Command&) {}
void ButtonView::applyVisualState(uint32_t, bool state) {
    ++visualUpdates;
    visualState = state;
}

int main() {
    DeviceConfiguration config;
    config.reportTemplates.push_back({"report", "Button", "button"});
    {
        ButtonView view;
        view.begin(config);
        lv_event_t event{&view};
        view.matrixEventCb(&event);
        view.matrixEventCb(&event); // Multiple outstanding flashes retain their behavior.
        assert(reports == 2 && liveTimers() == 2 && visualState);
    }
    assert(liveTimers() == 0);
    unsigned before = visualUpdates;
    expireTimers(); // Equivalent to next LVGL pass after config destroyed the view.
    assert(visualUpdates == before);
    {
        ButtonView view;
        view.begin(config);
        lv_event_t event{&view};
        view.matrixEventCb(&event);
        view._model.active[0] = true; // Latest inbound state must win at flash expiry.
        expireTimers();
        assert(visualState && liveTimers() == 0 && view._flashTimers.empty());
        view.matrixEventCb(&event);
        view.begin(config); // Reinitialization also invalidates slot addresses.
        assert(liveTimers() == 0);
        selectedButton = LV_BUTTONMATRIX_BUTTON_NONE;
        view.matrixEventCb(&event);
        assert(liveTimers() == 0);
    } // Must not double-delete already expired timers.
}
