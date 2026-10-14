#include "view_test.h"

using namespace riot2::views;

int main() {
    auto core = ButtonModel::buildTemplate("RIoT2.Ard.M5Core2.Node.ButtonView");
    CHECK(core.name == "Button View" && core.classFullName == "RIoT2.Ard.M5Core2.Node.ButtonView");
    CHECK(core.deviceParameters.size() == 2 && core.deviceParameters[0].first == "header");
    CHECK(core.deviceParameters[0].second == "Buttons" && core.deviceParameters[1].first == "subHeader");
    CHECK(core.deviceParameters[1].second == "tap a button");
    CHECK(core.commandTemplates.size() == 2 && core.reportTemplates.size() == 2);
    CHECK(core.commandTemplates[1].name == "Button btn-2" && core.commandTemplates[1].address == "btn-2");
    CHECK(core.reportTemplates[0].name == "Button btn-1" && core.reportTemplates[0].address == "btn-1");
    CHECK(core.commandTemplates[0].type == riot2::ValueType::Boolean);
    CHECK(core.reportTemplates[0].type == riot2::ValueType::Boolean && core.reportTemplates[0].parameters.empty());
    auto dial = ButtonModel::buildTemplate("RIoT2.Ard.M5Dial.Node.ButtonView", {{"icon", "bulb"}});
    CHECK(dial.classFullName == "RIoT2.Ard.M5Dial.Node.ButtonView" && dial.reportTemplates.size() == 2);
    CHECK(dial.reportTemplates[1].parameters.size() == 1 && dial.reportTemplates[1].parameters[0].second == "bulb");
    CHECK(dial.reportTemplates[0].parameters[0].first == "icon");

    ButtonModel model;
    model.begin(core);
    CHECK(model.header() == "Buttons" && model.subHeader() == "tap a button");
    CHECK(model.size() == 2 && model.name(1) == "Button btn-2");
    CHECK(!model.isActive(0) && !model.isLit(0));

    // Header defaults to the configuration name.
    model.begin(withParameters(core, {}));
    CHECK(model.header() == "Button View" && model.subHeader() == "");

    TestCommand on(core.commandTemplates[1].id.c_str(), "true"), off(core.commandTemplates[1].id.c_str(), "0"),
        number(core.commandTemplates[0].id.c_str(), "2"), other("other", "true");
    CHECK(model.onCommand(on.command) == 1 && model.isActive(1) && model.isLit(1) && !model.isActive(0));
    CHECK(model.onCommand(off.command) == 1 && !model.isActive(1));
    CHECK(model.onCommand(number.command) == 0 && model.isActive(0));
    CHECK(model.onCommand(other.command) == -1);

    // A press reports "true" and lights the button for kFlashMs without changing its state.
    hostMillis = 1000;
    Report report;
    CHECK(model.press(1, report) && report.id == core.reportTemplates[1].id && report.value == "true");
    CHECK(model.isLit(1) && !model.isActive(1));
    hostMillis = 1000 + ButtonModel::kFlashMs - 1;
    CHECK(model.isLit(1));
    hostMillis = 1000 + ButtonModel::kFlashMs;
    CHECK(!model.isLit(1));
    model.onCommand(on.command);
    CHECK(model.isLit(1));
    CHECK(!model.press(2, report));

    // The flash survives millis() wrapping around.
    model.begin(core);
    hostMillis = static_cast<unsigned long>(-50);
    CHECK(model.press(0, report));
    hostMillis = 100;
    CHECK(model.isLit(0));
    hostMillis = 151;
    CHECK(!model.isLit(0));

    // At most four buttons, paired by address; a report without a matching command has none.
    DeviceConfiguration many = core;
    for (const char* address : {"btn-3", "btn-4", "btn-5"}) {
        many.reportTemplates.push_back(makeReport(address, address, riot2::ValueType::Boolean));
    }
    many.reportTemplates[2].address = "";
    model.begin(many);
    CHECK(model.size() == 4 && model.name(3) == "btn-4");
    TestCommand unpaired("", "true");
    CHECK(model.onCommand(unpaired.command) == -1);

    // A button without a report id neither reports nor flashes.
    DeviceConfiguration noId = core;
    noId.reportTemplates[0].id = "";
    model.begin(noId);
    CHECK(!model.press(0, report) && !model.isLit(0));

    ButtonModel empty;
    empty.begin(DeviceConfiguration());
    CHECK(empty.size() == 0 && !empty.press(0, report));
    std::printf("ButtonModel ok\n");
}
