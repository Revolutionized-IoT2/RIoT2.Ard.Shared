#pragma once

#include <Arduino.h>

#include "riot2/views/AlertContent.h"

namespace riot2 {
namespace views {

// A transient alert pushed by a command (e.g. doorbell, alarm): AlertContent with the defaults
// "Alert" title and "tap to acknowledge" subHeader. It never auto-dismisses; once shown it stays
// until acknowledged, since it's meant for things that need a deliberate acknowledgement.
class AlertModel {
public:
    void begin(const DeviceConfiguration& config);

    const String& title() const { return _content.title; }
    const String& message() const { return _content.message; }
    const String& subHeader() const { return _content.subHeader; }
    bool soundEnabled() const { return _content.soundEnabled; }

    // Takes the content of the command; the view routes only its own commands here.
    void onCommand(const Command& command);

    // The alert went on screen; it is unacknowledged until acknowledge().
    void show() { _acknowledged = false; }
    // Returns false when it was already acknowledged.
    bool acknowledge();
    // Also true before the first show(): nothing to show until a command actually arrives.
    bool isAcknowledged() const { return _acknowledged; }

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    AlertContent _content;
    bool _acknowledged = true;
};

}  // namespace views
}  // namespace riot2
