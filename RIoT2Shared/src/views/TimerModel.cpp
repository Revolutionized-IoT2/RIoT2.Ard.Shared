#include "riot2/views/TimerModel.h"

#include <cstdio>

namespace riot2 {
namespace views {

constexpr int TimerModel::kRingCount;
constexpr unsigned long TimerModel::kRingIntervalMs;

void TimerModel::begin(const DeviceConfiguration& config) {
    _name = config.name;
    _commandId = config.commandTemplates.empty() ? String() : config.commandTemplates.front().id;
    _reportId = config.reportTemplates.empty() ? String() : config.reportTemplates.front().id;

    _stepMinutes = static_cast<int>(intParameter(config, "stepMinutes", 1));
    if (_stepMinutes <= 0) {
        _stepMinutes = 1;
    }
    _maxMinutes = static_cast<int>(intParameter(config, "maxMinutes", 60));
    if (_maxMinutes < _stepMinutes) {
        _maxMinutes = _stepMinutes;
    }
    _minutes = clampMinutes(intParameter(config, "defaultMinutes", 5));
    _beepOnComplete = findParameter(config.deviceParameters, "beepOnComplete", "false").equalsIgnoreCase("true");

    _phase = Phase::Setting;
    _totalSeconds = 0;
    _ringsRemaining = 0;
}

int TimerModel::clampMinutes(long minutes) const {
    return static_cast<int>(minutes < _stepMinutes ? _stepMinutes : (minutes > _maxMinutes ? _maxMinutes : minutes));
}

long TimerModel::remainingSeconds() const {
    if (_phase != Phase::Running) {
        return 0;
    }
    // Unsigned, so a countdown across the millis() rollover still counts right.
    unsigned long elapsedSeconds = (millis() - _startMs) / 1000;
    return elapsedSeconds >= static_cast<unsigned long>(_totalSeconds) ? 0
                                                                       : _totalSeconds - static_cast<long>(elapsedSeconds);
}

String TimerModel::remainingText() const {
    long remaining = remainingSeconds();
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02ld:%02ld", remaining / 60, remaining % 60);
    return String(buffer);
}

void TimerModel::adjust(int delta) {
    if (_phase != Phase::Setting) {
        return;
    }
    _minutes = clampMinutes(static_cast<long>(_minutes) + (delta > 0 ? _stepMinutes : -_stepMinutes));
}

bool TimerModel::onCommand(const Command& command) {
    if (_phase != Phase::Setting || _commandId.length() == 0 || command.id != _commandId) {
        return false;
    }
    _minutes = clampMinutes(commandInt(command));
    return true;
}

bool TimerModel::start() {
    if (_phase != Phase::Setting) {
        return false;
    }
    _totalSeconds = static_cast<long>(_minutes) * 60;
    _startMs = millis();
    _phase = Phase::Running;
    _ringsRemaining = 0;
    return true;
}

bool TimerModel::cancel() {
    if (_phase != Phase::Running) {
        return false;
    }
    _phase = Phase::Setting;
    _ringsRemaining = 0;
    return true;
}

bool TimerModel::dismiss() {
    if (_phase != Phase::Done) {
        return false;
    }
    _phase = Phase::Setting;
    _ringsRemaining = 0;
    return true;
}

bool TimerModel::finishIfElapsed() {
    if (_phase != Phase::Running || remainingSeconds() > 0) {
        return false;
    }
    _phase = Phase::Done;
    _ringsRemaining = _beepOnComplete ? kRingCount : 0;
    return true;
}

bool TimerModel::completionReport(Report& report) const {
    if (_reportId.length() == 0) {
        return false;
    }
    report = Report{_reportId, "0"};
    return true;
}

bool TimerModel::ringDue() {
    if (_ringsRemaining <= 0 || (_ringsRemaining != kRingCount && millis() - _lastRingMs < kRingIntervalMs)) {
        return false;
    }
    return takeRing();
}

bool TimerModel::takeRing() {
    if (_ringsRemaining <= 0) {
        return false;
    }
    --_ringsRemaining;
    _lastRingMs = millis();
    return true;
}

DeviceConfiguration TimerModel::buildTemplate(const String& classFullName, bool beepParameter) {
    DeviceConfiguration config = makeTemplate(classFullName, "Timer View");
    config.deviceParameters = {{"defaultMinutes", "5"}, {"stepMinutes", "1"}, {"maxMinutes", "60"}};
    if (beepParameter) {
        config.deviceParameters.push_back({"beepOnComplete", "false"});
    }
    config.commandTemplates.push_back(makeCommand("Timer", "timer-1", riot2::ValueType::Number));
    config.reportTemplates.push_back(makeReport("Timer", "timer-1", riot2::ValueType::Number));
    return config;
}

}  // namespace views
}  // namespace riot2
