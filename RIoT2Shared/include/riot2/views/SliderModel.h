#pragma once

#include <Arduino.h>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// A number in [min, max] in steps of `step` (deviceParameters "min", "max", "step", "unit"; default
// 0, 100, 1, ""), bound to the first command and report template. A command sets the value without
// a report; a value the user confirms produces a report.
class SliderModel {
public:
    void begin(const DeviceConfiguration& config);

    const String& name() const { return _name; }
    const String& unit() const { return _unit; }
    int minimum() const { return _min; }
    int maximum() const { return _max; }
    int step() const { return _step; }
    int value() const { return _value; }
    bool canReport() const { return _reportId.length() > 0; }

    // The nearest step from min that is within [min, max].
    int snap(int value) const;
    int clamp(int value) const;

    // Applies a command for this slider. Returns false for another command id.
    bool onCommand(const Command& command);

    // The user confirmed `value` (snapped). Returns false without a report template.
    bool set(int value, Report& report);

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    String _name;
    String _commandId;
    String _reportId;
    String _unit;
    int _min = 0;
    int _max = 100;
    int _step = 1;
    int _value = 0;
};

}  // namespace views
}  // namespace riot2
