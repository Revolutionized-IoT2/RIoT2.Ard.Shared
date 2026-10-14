#include "riot2/views/SceneSelectorModel.h"

namespace riot2 {
namespace views {

void SceneSelectorModel::begin(const DeviceConfiguration& config) {
    _scenes = config.reportTemplates;
}

int SceneSelectorModel::wrap(int index) const {
    if (_scenes.empty()) {
        return 0;
    }
    int count = static_cast<int>(_scenes.size());
    return ((index % count) + count) % count;
}

bool SceneSelectorModel::select(size_t index, Report& report) const {
    if (index >= _scenes.size()) {
        return false;
    }
    report = Report{_scenes[index].id, "true"};
    return true;
}

DeviceConfiguration SceneSelectorModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Scene Selector View");
    for (const char* sceneName : {"Movie Night", "Good Morning", "Good Night"}) {
        config.reportTemplates.push_back(makeReport(sceneName, "", riot2::ValueType::Text));
    }
    return config;
}

}  // namespace views
}  // namespace riot2
