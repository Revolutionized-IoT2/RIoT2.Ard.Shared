#include "riot2/views/ButtonModel.h"

namespace riot2 {
namespace views {

void ButtonModel::begin(const DeviceConfiguration& config) {
    _header = findParameter(config.deviceParameters, "header", config.name);
    _subHeader = findParameter(config.deviceParameters, "subHeader", "");
    _slots.clear();
    for (const auto& slot : pairByAddress(config, kMaxSlots)) {
        Button button;
        button.slot = slot;
        _slots.push_back(button);
    }
}

bool ButtonModel::isLit(size_t index) const {
    const Button& button = _slots[index];
    return button.active || (button.pressed && millis() - button.pressedAtMs < kFlashMs);
}

int ButtonModel::onCommand(const Command& command) {
    for (size_t i = 0; i < _slots.size(); ++i) {
        if (_slots[i].slot.hasCommand && _slots[i].slot.command.id == command.id) {
            _slots[i].active = commandBool(command);
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool ButtonModel::press(size_t index, Report& report) {
    if (index >= _slots.size() || _slots[index].slot.report.id.length() == 0) {
        return false;
    }
    _slots[index].pressed = true;
    _slots[index].pressedAtMs = millis();
    report = Report{_slots[index].slot.report.id, boolLiteral(true)};
    return true;
}

DeviceConfiguration ButtonModel::buildTemplate(const String& classFullName, const ParameterList& reportParameters) {
    DeviceConfiguration config = makeTemplate(classFullName, "Button View");
    config.deviceParameters = {{"header", "Buttons"}, {"subHeader", "tap a button"}};
    for (const char* address : {"btn-1", "btn-2"}) {
        String name = String("Button ") + address;
        config.commandTemplates.push_back(makeCommand(name, address, riot2::ValueType::Boolean));
        ReportTemplate report = makeReport(name, address, riot2::ValueType::Boolean);
        report.parameters = reportParameters;
        config.reportTemplates.push_back(report);
    }
    return config;
}

}  // namespace views
}  // namespace riot2
