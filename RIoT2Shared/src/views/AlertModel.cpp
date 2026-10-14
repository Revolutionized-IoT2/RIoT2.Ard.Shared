#include "riot2/views/AlertModel.h"

namespace riot2 {
namespace views {

namespace {
const char* const kAlertTitle = "Alert";
const char* const kAlertSubHeader = "tap to acknowledge";
}  // namespace

void AlertModel::begin(const DeviceConfiguration& config) {
    (void)config;
    _content = AlertContent();
    _content.title = kAlertTitle;
    _content.subHeader = kAlertSubHeader;
    _acknowledged = true;
}

void AlertModel::onCommand(const Command& command) {
    _content = AlertContent::parse(command, kAlertTitle, kAlertSubHeader);
}

bool AlertModel::acknowledge() {
    if (_acknowledged) {
        return false;
    }
    _acknowledged = true;
    return true;
}

DeviceConfiguration AlertModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Alert View");
    // value is { title, message, subHeader, soundEnabled }
    config.commandTemplates.push_back(makeCommand("Alert", "alert-1", riot2::ValueType::Entity));
    return config;
}

}  // namespace views
}  // namespace riot2
