#pragma once

#include <Arduino.h>

#include <vector>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// One or two on/off switches. Each pairs a report template with the command template of the
// same address; a command sets the switch without a report, a user change produces a report.
class ToggleModel {
public:
    static constexpr size_t kMaxSlots = 2;

    void begin(const DeviceConfiguration& config);

    size_t size() const { return _slots.size(); }
    const String& name(size_t index) const { return _slots[index].slot.report.name; }
    bool isActive(size_t index) const { return _slots[index].active; }

    // Applies a command to its switch. Returns the switch's index, or -1 when no switch owns it.
    int onCommand(const Command& command);

    // The user set a switch. Returns false (and changes nothing) for an unknown switch or one
    // without a report id; otherwise `report` is the report to publish.
    bool set(size_t index, bool active, Report& report);
    bool toggle(size_t index, Report& report);

    // `switches` relay-N switches; one switch is named "Toggle", several "Toggle relay-N".
    static DeviceConfiguration buildTemplate(const String& classFullName, size_t switches);

private:
    struct Switch {
        Slot slot;
        bool active = false;
    };
    std::vector<Switch> _slots;
};

}  // namespace views
}  // namespace riot2
