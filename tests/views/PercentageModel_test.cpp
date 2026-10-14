#include "view_test.h"

using namespace riot2::views;

int main() {
    for (const char* board : {"RIoT2.Ard.M5Core2.Node.PercentageView", "RIoT2.Ard.M5Dial.Node.PercentageView"}) {
        auto config = PercentageModel::buildTemplate(board);
        CHECK(config.name == "Percentage View" && config.classFullName == board);
        CHECK(config.id.length() > 0 && config.deviceParameters.empty());
        CHECK(config.commandTemplates.size() == 1 && config.reportTemplates.size() == 1);
        CHECK(config.commandTemplates[0].name == "Percentage" && config.commandTemplates[0].address == "dimmer-1");
        CHECK(config.commandTemplates[0].type == riot2::ValueType::Number);
        CHECK(config.reportTemplates[0].name == "Percentage" && config.reportTemplates[0].address == "dimmer-1");
        CHECK(config.reportTemplates[0].type == riot2::ValueType::Number);
        CHECK(config.commandTemplates[0].id != config.reportTemplates[0].id);
    }

    auto config = PercentageModel::buildTemplate("RIoT2.Ard.M5Dial.Node.PercentageView");
    config.name = "Dimmer";
    PercentageModel model;
    model.begin(config);
    CHECK(model.name() == "Dimmer" && model.value() == 0 && model.canReport());

    CHECK(PercentageModel::clamp(-5) == 0 && PercentageModel::clamp(150) == 100 && PercentageModel::clamp(42) == 42);

    // Commands are clamped; another command id changes nothing.
    TestCommand fifty(config.commandTemplates[0].id.c_str(), "50"), high(config.commandTemplates[0].id.c_str(), "250"),
        low(config.commandTemplates[0].id.c_str(), "-3"), other("other", "70");
    CHECK(model.onCommand(fifty.command) && model.value() == 50);
    CHECK(model.onCommand(high.command) && model.value() == 100);
    CHECK(model.onCommand(low.command) && model.value() == 0);
    CHECK(!model.onCommand(other.command) && model.value() == 0);

    Report report;
    CHECK(model.set(64, report) && report.id == config.reportTemplates[0].id && report.value == "64");
    CHECK(model.set(101, report) && report.value == "100" && model.value() == 100);

    // Without a report template the value still follows the user, without a report.
    DeviceConfiguration noReport = config;
    noReport.reportTemplates.clear();
    model.begin(noReport);
    report = Report();
    CHECK(!model.canReport() && !model.set(30, report) && model.value() == 30 && report.id.length() == 0);

    PercentageModel empty;
    empty.begin(DeviceConfiguration());
    CHECK(!empty.onCommand(other.command) && !empty.canReport());
    std::printf("PercentageModel ok\n");
}
