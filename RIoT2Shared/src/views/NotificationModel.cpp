#include "riot2/views/NotificationModel.h"

namespace riot2 {
namespace views {

constexpr unsigned long NotificationModel::kDefaultDurationMs;

namespace {
const char* const kNotificationTitle = "Notification";
}  // namespace

void NotificationModel::begin(const DeviceConfiguration& config) {
    _content = AlertContent();
    _content.title = kNotificationTitle;
    long duration = intParameter(config, "durationMs", 0);
    _durationMs = duration > 0 ? static_cast<unsigned long>(duration) : kDefaultDurationMs;
    _dismissed = true;
}

void NotificationModel::onCommand(const Command& command) {
    _content = AlertContent::parse(command, kNotificationTitle, "");
}

void NotificationModel::show() {
    _dismissed = false;
    _shownAtMs = millis();
}

bool NotificationModel::isExpired() const {
    return _dismissed || (millis() - _shownAtMs) >= _durationMs;
}

float NotificationModel::remaining() const {
    unsigned long elapsed = millis() - _shownAtMs;
    float left = 1.f - static_cast<float>(elapsed) / static_cast<float>(_durationMs);
    return left < 0.f ? 0.f : (left > 1.f ? 1.f : left);
}

DeviceConfiguration NotificationModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Notification View");
    config.deviceParameters = {{"durationMs", "4000"}};
    // value is { title, message, subHeader, soundEnabled }
    config.commandTemplates.push_back(makeCommand("Notification", "notice-1", riot2::ValueType::Entity));
    return config;
}

}  // namespace views
}  // namespace riot2
