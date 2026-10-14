#pragma once

#include <Arduino.h>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// What an alert-like command (AlertModel, NotificationModel) shows. The Command's `value` is
// expected to be an object with "title", "message" (falling back to "text") and "subHeader"
// fields; a plain string is used directly as the message instead. Anything else (or a missing
// key) falls back to the supplied defaults and an empty message.
//
// The object may also include a "soundEnabled" field (default false). Sent per-command rather
// than as a commandTemplate parameter, since RIoT2.Core doesn't support parameters on
// commandTemplates.
struct AlertContent {
    String title;
    String message;
    String subHeader;
    bool soundEnabled = false;

    static AlertContent parse(const Command& command, const String& defaultTitle, const String& defaultSubHeader);
};

}  // namespace views
}  // namespace riot2
