#include "riot2/views/SliderModel.h"

namespace riot2 {
namespace views {

void SliderModel::begin(const DeviceConfiguration& config) {
    _name = config.name;
    _commandId = config.commandTemplates.empty() ? String() : config.commandTemplates.front().id;
    _reportId = config.reportTemplates.empty() ? String() : config.reportTemplates.front().id;
    _min = static_cast<int>(intParameter(config, "min", 0));
    _max = static_cast<int>(intParameter(config, "max", 100));
    _step = static_cast<int>(intParameter(config, "step", 1));
    if (_step <= 0) {
        _step = 1;
    }
    if (_max <= _min) {
        _max = _min + 1;
    }
    _unit = findParameter(config.deviceParameters, "unit", "");
    _value = _min;
}

int SliderModel::clamp(int value) const {
    return value < _min ? _min : (value > _max ? _max : value);
}

int SliderModel::snap(int value) const {
    // The nearest step (halves round up, as lroundf did), and never past max: when max - min isn't
    // a multiple of step, max itself is off the grid. snap(snap(x)) == snap(x), so a snapped
    // value is shown and reported unchanged.
    long offset = static_cast<long>(clamp(value)) - _min;
    long snapped = _min + (offset + _step / 2) / _step * _step;
    if (snapped > _max) {
        snapped -= _step;
    }
    return static_cast<int>(snapped);
}

bool SliderModel::onCommand(const Command& command) {
    if (_commandId.length() == 0 || command.id != _commandId) {
        return false;
    }
    _value = snap(commandInt(command));
    return true;
}

bool SliderModel::set(int value, Report& report) {
    if (!canReport()) {
        return false;
    }
    _value = snap(value);
    report = Report{_reportId, intLiteral(_value)};
    return true;
}

DeviceConfiguration SliderModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Slider View");
    config.deviceParameters = {{"min", "0"}, {"max", "100"}, {"step", "1"}, {"unit", ""}};
    config.commandTemplates.push_back(makeCommand("Slider", "slider-1", riot2::ValueType::Number));
    config.reportTemplates.push_back(makeReport("Slider", "slider-1", riot2::ValueType::Number));
    return config;
}

}  // namespace views
}  // namespace riot2
