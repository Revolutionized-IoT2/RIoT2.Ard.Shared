#pragma once

#include <Arduino.h>

#include <vector>

#include <riot2/Command.h>
#include <riot2/DeviceConfiguration.h>
#include <riot2/Report.h>

// Display-independent helpers for the view models in riot2/views/ (plan M5). The boards' views
// own a model and only render it and turn input into model calls; everything here builds on the
// host too, so tests/test_view_models.py can test it without a board SDK.
namespace riot2 {
namespace views {

// A report template and the command template with the same `address`, as the boards pair them.
struct Slot {
    ReportTemplate report;
    CommandTemplate command;
    bool hasCommand = false;
};

// One slot per report template, in order, at most `maxSlots`; a report without an address gets no
// command.
std::vector<Slot> pairByAddress(const DeviceConfiguration& config, size_t maxSlots);

// Index of the slot whose command has `commandId`, or -1.
int findSlotByCommand(const std::vector<Slot>& slots, const String& commandId);

// Command values as the boards read them: a JSON bool, or a number that is not 0.
bool commandBool(const Command& command);
int commandInt(const Command& command);

// A string value as it is; anything else as JSON ("21.5", "true", "{...}").
String commandText(const Command& command);

// JSON literals for Report::value.
String boolLiteral(bool value);
String intLiteral(long value);
String stringLiteral(const String& value);

// Integer device parameter with a fallback.
long intParameter(const DeviceConfiguration& config, const char* key, long defaultValue);

// Default configurations for /api/device/configuration/templates: fresh ids from riot2::newId().
DeviceConfiguration makeTemplate(const String& classFullName, const String& name);
CommandTemplate makeCommand(const String& name, const String& address, int type);
ReportTemplate makeReport(const String& name, const String& address, int type);

}  // namespace views
}  // namespace riot2
