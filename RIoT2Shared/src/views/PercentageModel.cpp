#include "riot2/views/PercentageModel.h"

namespace riot2 {
namespace views {

constexpr int PercentageModel::kMinimum;
constexpr int PercentageModel::kMaximum;

void PercentageModel::begin(const DeviceConfiguration& config) {
    _name = config.name;
    _commandId = config.commandTemplates.empty() ? String() : config.commandTemplates.front().id;
    _reportId = config.reportTemplates.empty() ? String() : config.reportTemplates.front().id;
    _value = kMinimum;
}

int PercentageModel::clamp(int value) {
    return value < kMinimum ? kMinimum : (value > kMaximum ? kMaximum : value);
}

bool PercentageModel::onCommand(const Command& command) {
    if (_commandId.length() == 0 || command.id != _commandId) {
        return false;
    }
    _value = clamp(commandInt(command));
    return true;
}

bool PercentageModel::set(int value, Report& report) {
    _value = clamp(value);
    if (!canReport()) {
        return false;
    }
    report = Report{_reportId, intLiteral(_value)};
    return true;
}

DeviceConfiguration PercentageModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Percentage View");
    config.commandTemplates.push_back(makeCommand("Percentage", "dimmer-1", riot2::ValueType::Number));
    config.reportTemplates.push_back(makeReport("Percentage", "dimmer-1", riot2::ValueType::Number));
    return config;
}

}  // namespace views
}  // namespace riot2
