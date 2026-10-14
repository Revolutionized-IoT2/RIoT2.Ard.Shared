#pragma once

#include <Arduino.h>

#include <vector>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// 1-4 momentary buttons. Each pairs a report template with the command template of the same
// address. Pressing a button reports "true" and lights it for kFlashMs as a tap acknowledgement;
// a command sets whether the button stays lit (highlighted) without a report. The header and
// subheader come from deviceParameters "header" (default: the configuration name) and
// "subHeader".
class ButtonModel {
public:
    static constexpr size_t kMaxSlots = 4;
    static constexpr unsigned long kFlashMs = 200;

    void begin(const DeviceConfiguration& config);

    const String& header() const { return _header; }
    const String& subHeader() const { return _subHeader; }
    size_t size() const { return _slots.size(); }
    const String& name(size_t index) const { return _slots[index].slot.report.name; }
    // The state the last command set.
    bool isActive(size_t index) const { return _slots[index].active; }
    // Active, or pressed less than kFlashMs ago.
    bool isLit(size_t index) const;

    // Applies a command to its button. Returns the button's index, or -1 when no button owns it.
    int onCommand(const Command& command);

    // The user pressed a button. Returns false (and changes nothing) for an unknown button or one
    // without a report id; otherwise `report` is the report to publish.
    bool press(size_t index, Report& report);

    // Buttons btn-1 and btn-2; each report template gets `reportParameters`.
    static DeviceConfiguration buildTemplate(const String& classFullName,
                                             const ParameterList& reportParameters = ParameterList());

private:
    struct Button {
        Slot slot;
        bool active = false;
        bool pressed = false;
        unsigned long pressedAtMs = 0;
    };
    String _header;
    String _subHeader;
    std::vector<Button> _slots;
};

}  // namespace views
}  // namespace riot2
