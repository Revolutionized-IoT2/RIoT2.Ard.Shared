#pragma once

#include <Arduino.h>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// A 0-100% value bound to the first command and report template (by list order); no
// deviceParameters. A command sets the value without a report; a value the user confirms produces
// a report.
class PercentageModel {
public:
    static constexpr int kMinimum = 0;
    static constexpr int kMaximum = 100;

    void begin(const DeviceConfiguration& config);

    const String& name() const { return _name; }
    int value() const { return _value; }
    bool canReport() const { return _reportId.length() > 0; }

    static int clamp(int value);

    // Applies a command for this view (clamped). Returns false for another command id.
    bool onCommand(const Command& command);

    // The user set `value` (clamped); returns false (with the value still set) without a report
    // template, otherwise `report` is the report to publish.
    bool set(int value, Report& report);

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    String _name;
    String _commandId;
    String _reportId;
    int _value = 0;
};

}  // namespace views
}  // namespace riot2
