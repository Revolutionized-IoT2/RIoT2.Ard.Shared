#include "view_test.h"

using namespace riot2::views;

int main() {
    auto core = ToggleModel::buildTemplate("RIoT2.Ard.M5Core2.Node.ToggleView", 2);
    CHECK(core.name == "Toggle View" && core.classFullName == "RIoT2.Ard.M5Core2.Node.ToggleView");
    CHECK(core.commandTemplates.size() == 2 && core.reportTemplates.size() == 2);
    CHECK(core.reportTemplates[1].name == "Toggle relay-2" && core.reportTemplates[1].address == "relay-2");
    CHECK(core.commandTemplates[0].type == riot2::ValueType::Boolean);
    auto dial = ToggleModel::buildTemplate("RIoT2.Ard.M5Dial.Node.ToggleView", 1);
    CHECK(dial.reportTemplates.size() == 1 && dial.reportTemplates[0].name == "Toggle");
    CHECK(dial.commandTemplates[0].address == "relay-1");

    // At most two switches, paired by address.
    core.reportTemplates.push_back(makeReport("Third", "relay-3", riot2::ValueType::Boolean));
    ToggleModel model;
    model.begin(core);
    CHECK(model.size() == 2);
    CHECK(model.name(0) == "Toggle relay-1");
    CHECK(!model.isActive(0) && !model.isActive(1));

    TestCommand on(core.commandTemplates[1].id.c_str(), "true"), off(core.commandTemplates[1].id.c_str(), "0"),
        other("other", "true");
    CHECK(model.onCommand(on.command) == 1);
    CHECK(model.isActive(1) && !model.isActive(0));
    CHECK(model.onCommand(off.command) == 1 && !model.isActive(1));
    CHECK(model.onCommand(other.command) == -1);

    Report report;
    CHECK(model.toggle(0, report));
    CHECK(model.isActive(0) && report.id == core.reportTemplates[0].id && report.value == "true");
    CHECK(model.set(0, false, report) && report.value == "false" && !model.isActive(0));
    CHECK(!model.toggle(5, report));

    // A switch without a report id doesn't change or report.
    DeviceConfiguration noId = core;
    noId.reportTemplates[0].id = "";
    model.begin(noId);
    CHECK(!model.toggle(0, report) && !model.isActive(0));

    ToggleModel empty;
    empty.begin(DeviceConfiguration());
    CHECK(empty.size() == 0 && !empty.toggle(0, report));
    std::printf("ToggleModel ok\n");
}
