#pragma once

#include <Arduino.h>

#include <vector>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// One scene per report template, in order. Selecting a scene produces Report{id, "true"} for its
// report template; the view takes no commands.
class SceneSelectorModel {
public:
    void begin(const DeviceConfiguration& config);

    size_t size() const { return _scenes.size(); }
    bool empty() const { return _scenes.empty(); }
    const String& name(size_t index) const { return _scenes[index].name; }

    // `index` wrapped around the scene list (e.g. -1 is the last scene); 0 without scenes.
    int wrap(int index) const;

    // The user selected a scene. Returns false for an unknown index; otherwise `report` is the
    // report to publish.
    bool select(size_t index, Report& report) const;

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    std::vector<ReportTemplate> _scenes;
};

}  // namespace views
}  // namespace riot2
