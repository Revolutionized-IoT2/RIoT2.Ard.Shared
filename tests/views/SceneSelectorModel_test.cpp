#include "view_test.h"

using namespace riot2::views;

int main() {
    for (const char* board : {"RIoT2.Ard.M5Core2.Node.SceneSelectorView", "RIoT2.Ard.M5Dial.Node.SceneSelectorView"}) {
        auto config = SceneSelectorModel::buildTemplate(board);
        CHECK(config.name == "Scene Selector View" && config.classFullName == board);
        CHECK(config.commandTemplates.empty() && config.deviceParameters.empty());
        CHECK(config.reportTemplates.size() == 3);
        CHECK(config.reportTemplates[0].name == "Movie Night" && config.reportTemplates[1].name == "Good Morning" &&
              config.reportTemplates[2].name == "Good Night");
        for (const auto& report : config.reportTemplates) {
            CHECK(report.type == riot2::ValueType::Text && report.address.length() == 0 && report.id.length() > 0);
        }
    }

    auto config = SceneSelectorModel::buildTemplate("RIoT2.Ard.M5Dial.Node.SceneSelectorView");
    SceneSelectorModel model;
    model.begin(config);
    CHECK(model.size() == 3 && !model.empty() && model.name(1) == "Good Morning");

    CHECK(model.wrap(0) == 0 && model.wrap(2) == 2 && model.wrap(3) == 0 && model.wrap(-1) == 2 && model.wrap(-4) == 2);

    Report report;
    CHECK(model.select(2, report) && report.id == config.reportTemplates[2].id && report.value == "true");
    CHECK(!model.select(3, report));

    SceneSelectorModel empty;
    empty.begin(DeviceConfiguration());
    CHECK(empty.empty() && empty.size() == 0 && empty.wrap(-1) == 0 && !empty.select(0, report));
    std::printf("SceneSelectorModel ok\n");
}
