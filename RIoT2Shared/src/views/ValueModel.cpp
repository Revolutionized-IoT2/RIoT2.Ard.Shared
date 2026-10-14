#include "riot2/views/ValueModel.h"

namespace riot2 {
namespace views {

void ValueModel::begin(const DeviceConfiguration& config) {
    _slots.clear();
    for (size_t i = 0; i < config.commandTemplates.size() && i < kMaxSlots; ++i) {
        Entry entry;
        entry.id = config.commandTemplates[i].id;
        entry.name = config.commandTemplates[i].name;
        entry.unit = findParameter(config.deviceParameters, String("unit") + intLiteral(static_cast<long>(i + 1)));
        _slots.push_back(entry);
    }
}

int ValueModel::onCommand(const Command& command) {
    for (size_t i = 0; i < _slots.size(); ++i) {
        if (_slots[i].id == command.id) {
            _slots[i].value = commandText(command);
            return static_cast<int>(i);
        }
    }
    return -1;
}

DeviceConfiguration ValueModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Value View");
    config.deviceParameters = {{"unit1", "\u00b0C"}, {"unit2", "%"}};
    for (const char* label : {"Value 1", "Value 2"}) {
        config.commandTemplates.push_back(makeCommand(label, "", riot2::ValueType::Number));
    }
    return config;
}

}  // namespace views
}  // namespace riot2
