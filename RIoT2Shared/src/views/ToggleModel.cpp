#include "riot2/views/ToggleModel.h"

namespace riot2 {
namespace views {

void ToggleModel::begin(const DeviceConfiguration& config) {
    _slots.clear();
    for (const auto& slot : pairByAddress(config, kMaxSlots)) {
        Switch entry;
        entry.slot = slot;
        _slots.push_back(entry);
    }
}

int ToggleModel::onCommand(const Command& command) {
    for (size_t i = 0; i < _slots.size(); ++i) {
        if (_slots[i].slot.hasCommand && _slots[i].slot.command.id == command.id) {
            _slots[i].active = commandBool(command);
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool ToggleModel::set(size_t index, bool active, Report& report) {
    if (index >= _slots.size() || _slots[index].slot.report.id.length() == 0) {
        return false;
    }
    _slots[index].active = active;
    report = Report{_slots[index].slot.report.id, boolLiteral(active)};
    return true;
}

bool ToggleModel::toggle(size_t index, Report& report) {
    return index < _slots.size() && set(index, !_slots[index].active, report);
}

DeviceConfiguration ToggleModel::buildTemplate(const String& classFullName, size_t switches) {
    DeviceConfiguration config = makeTemplate(classFullName, "Toggle View");
    for (size_t i = 1; i <= switches; ++i) {
        String address = String("relay-") + intLiteral(static_cast<long>(i));
        String name = switches == 1 ? String("Toggle") : String("Toggle ") + address;
        config.commandTemplates.push_back(makeCommand(name, address, riot2::ValueType::Boolean));
        config.reportTemplates.push_back(makeReport(name, address, riot2::ValueType::Boolean));
    }
    return config;
}

}  // namespace views
}  // namespace riot2
