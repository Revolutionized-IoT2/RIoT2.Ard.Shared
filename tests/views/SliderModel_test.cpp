#include "view_test.h"

using namespace riot2::views;

int main() {
    auto config = SliderModel::buildTemplate("RIoT2.Ard.M5Dial.Node.SliderView");
    CHECK(config.name == "Slider View" && config.deviceParameters.size() == 4);
    CHECK(config.commandTemplates[0].address == "slider-1" && config.commandTemplates[0].type == riot2::ValueType::Number);

    SliderModel model;
    model.begin(withParameters(config, {{"min", "10"}, {"max", "50"}, {"step", "5"}, {"unit", "%"}}));
    CHECK(model.minimum() == 10 && model.maximum() == 50 && model.step() == 5 && model.unit() == "%");
    CHECK(model.value() == 10 && model.name() == "Slider View");
    CHECK(model.snap(12) == 10 && model.snap(13) == 15 && model.snap(99) == 50 && model.snap(-3) == 10);
    CHECK(model.clamp(60) == 50 && model.clamp(5) == 10 && model.clamp(33) == 33);

    // A max off the step grid is never shown or reported: snapping is idempotent.
    SliderModel offGrid;
    offGrid.begin(withParameters(config, {{"min", "0"}, {"max", "10"}, {"step", "3"}}));
    CHECK(offGrid.snap(10) == 9 && offGrid.snap(12) == 9 && offGrid.snap(9) == 9 && offGrid.snap(5) == 6);
    for (int v = -5; v <= 15; ++v) {
        CHECK(offGrid.snap(offGrid.snap(v)) == offGrid.snap(v));
    }

    TestCommand command(config.commandTemplates[0].id.c_str(), "27"), other("other", "40");
    CHECK(model.onCommand(command.command) && model.value() == 25);
    CHECK(!model.onCommand(other.command) && model.value() == 25);

    Report report;
    CHECK(model.set(42, report) && model.value() == 40);
    CHECK(report.id == config.reportTemplates[0].id && report.value == "40");

    // Bad parameters fall back to a usable range.
    model.begin(withParameters(config, {{"min", "5"}, {"max", "5"}, {"step", "0"}}));
    CHECK(model.minimum() == 5 && model.maximum() == 6 && model.step() == 1);
    model.begin(withParameters(config, {}));
    CHECK(model.minimum() == 0 && model.maximum() == 100 && model.step() == 1 && model.unit() == "");

    DeviceConfiguration noReport = config;
    noReport.reportTemplates.clear();
    model.begin(noReport);
    CHECK(!model.canReport() && !model.set(10, report) && model.value() == 0);
    std::printf("SliderModel ok\n");
}
