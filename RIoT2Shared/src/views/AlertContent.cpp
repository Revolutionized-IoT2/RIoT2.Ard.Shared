#include "riot2/views/AlertContent.h"

namespace riot2 {
namespace views {

namespace {
String alertContentField(const JsonVariantConst& value, const char* key, const String& fallback) {
    if (value.is<JsonObjectConst>()) {
        JsonVariantConst field = value.as<JsonObjectConst>()[key];
        if (field.is<const char*>()) {
            return String(field.as<const char*>());
        }
    }
    return fallback;
}

// Accepts a real JSON boolean (expected) or a "true"/"false" string, so it
// tolerates either representation from the orchestrator.
bool alertContentSoundEnabled(const JsonVariantConst& value) {
    if (value.is<JsonObjectConst>()) {
        JsonVariantConst field = value.as<JsonObjectConst>()["soundEnabled"];
        if (field.is<bool>()) {
            return field.as<bool>();
        }
        if (field.is<const char*>()) {
            return String(field.as<const char*>()).equalsIgnoreCase("true");
        }
    }
    return false;
}
}  // namespace

AlertContent AlertContent::parse(const Command& command, const String& defaultTitle,
                                 const String& defaultSubHeader) {
    AlertContent content;
    content.title = alertContentField(command.value, "title", defaultTitle);
    content.message = command.value.is<const char*>()
                          ? String(command.value.as<const char*>())
                          : alertContentField(command.value, "message", alertContentField(command.value, "text", ""));
    content.subHeader = alertContentField(command.value, "subHeader", defaultSubHeader);
    content.soundEnabled = alertContentSoundEnabled(command.value);
    return content;
}

}  // namespace views
}  // namespace riot2
