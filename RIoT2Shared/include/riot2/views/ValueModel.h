#pragma once

#include <Arduino.h>

#include <vector>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// Up to four read-only values, one per command template, with units from deviceParameters
// "unit1".."unit4". Commands set the values; the view never reports.
class ValueModel {
public:
    static constexpr size_t kMaxSlots = 4;

    void begin(const DeviceConfiguration& config);

    size_t size() const { return _slots.size(); }
    const String& name(size_t index) const { return _slots[index].name; }
    const String& unit(size_t index) const { return _slots[index].unit; }
    // The value without its unit; "--" until the first command.
    const String& value(size_t index) const { return _slots[index].value; }
    // The value followed by its unit, as the boards show it.
    String text(size_t index) const { return _slots[index].value + _slots[index].unit; }

    // Returns the index of the value the command set, or -1.
    int onCommand(const Command& command);

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    struct Entry {
        String id;
        String name;
        String unit;
        String value = "--";
    };
    std::vector<Entry> _slots;
};

}  // namespace views
}  // namespace riot2
