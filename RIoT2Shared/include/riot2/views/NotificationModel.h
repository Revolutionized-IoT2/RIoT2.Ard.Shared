#pragma once

#include <Arduino.h>

#include "riot2/views/AlertContent.h"

namespace riot2 {
namespace views {

// A transient notification pushed by a command (e.g. a heads-up status update): AlertContent
// with the default "Notification" title and an empty subHeader. Unlike AlertModel it doesn't need
// an acknowledgement: it expires `durationMs` after show() (deviceParameter "durationMs", default
// 4 seconds), or earlier when dismissed.
class NotificationModel {
public:
    static constexpr unsigned long kDefaultDurationMs = 4000;

    void begin(const DeviceConfiguration& config);

    const String& title() const { return _content.title; }
    const String& message() const { return _content.message; }
    const String& subHeader() const { return _content.subHeader; }
    bool soundEnabled() const { return _content.soundEnabled; }
    unsigned long durationMs() const { return _durationMs; }

    // Takes the content of the command; the view routes only its own commands here.
    void onCommand(const Command& command);

    // The notification went on screen now (millis()).
    void show();
    void dismiss() { _dismissed = true; }
    // Dismissed or timed out; also true before the first show(): nothing to show until a command
    // actually arrives.
    bool isExpired() const;
    // Share of durationMs left, from 1 right after show() down to 0.
    float remaining() const;

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    AlertContent _content;
    unsigned long _durationMs = kDefaultDurationMs;
    unsigned long _shownAtMs = 0;
    bool _dismissed = true;
};

}  // namespace views
}  // namespace riot2
