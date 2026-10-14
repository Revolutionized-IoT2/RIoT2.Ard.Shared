#include "view_test.h"

using namespace riot2::views;

int main() {
    auto config = ValueModel::buildTemplate("RIoT2.Ard.M5Core2.Node.ValueView");
    CHECK(config.name == "Value View" && config.commandTemplates.size() == 2 && config.reportTemplates.empty());
    CHECK(findParameter(config.deviceParameters, "unit1") == "\u00b0C");

    for (int i = 3; i <= 5; ++i) {
        config.commandTemplates.push_back(makeCommand("Value " + String(i), "", riot2::ValueType::Number));
    }
    ValueModel model;
    model.begin(config);
    CHECK(model.size() == 4);
    CHECK(model.name(1) == "Value 2" && model.unit(1) == "%" && model.unit(2) == "");
    CHECK(model.value(0) == "--" && model.text(1) == "--%");

    TestCommand number(config.commandTemplates[0].id.c_str(), "21.5"),
        text(config.commandTemplates[1].id.c_str(), "\"high\""), fifth(config.commandTemplates[4].id.c_str(), "1");
    CHECK(model.onCommand(number.command) == 0 && model.text(0) == "21.5\u00b0C");
    CHECK(model.onCommand(text.command) == 1 && model.value(1) == "high");
    CHECK(model.onCommand(fifth.command) == -1);  // beyond the four values
    std::printf("ValueModel ok\n");
}
