#include "view_test.h"

using namespace riot2::views;

int main() {
    for (const char* board : {"RIoT2.Ard.M5Core2.Node.AlertView", "RIoT2.Ard.M5Dial.Node.AlertView"}) {
        auto config = AlertModel::buildTemplate(board);
        CHECK(config.name == "Alert View" && config.classFullName == board && config.id.length() > 0);
        CHECK(config.reportTemplates.empty() && config.deviceParameters.empty());
        CHECK(config.commandTemplates.size() == 1 && config.commandTemplates[0].name == "Alert");
        CHECK(config.commandTemplates[0].address == "alert-1" &&
              config.commandTemplates[0].type == riot2::ValueType::Entity);
    }

    AlertModel model;
    model.begin(AlertModel::buildTemplate("RIoT2.Ard.M5Dial.Node.AlertView"));
    CHECK(model.title() == "Alert" && model.message().length() == 0 && model.subHeader() == "tap to acknowledge");
    CHECK(!model.soundEnabled() && model.isAcknowledged());

    TestCommand doorbell("a", R"({"title":"Doorbell","message":"Front door","soundEnabled":true})");
    model.onCommand(doorbell.command);
    CHECK(model.title() == "Doorbell" && model.message() == "Front door" && model.subHeader() == "tap to acknowledge");
    CHECK(model.soundEnabled() && model.isAcknowledged());

    model.show();
    CHECK(!model.isAcknowledged());
    CHECK(model.acknowledge() && model.isAcknowledged());
    CHECK(!model.acknowledge());

    // The next command replaces the whole content.
    TestCommand plain("a", R"("Smoke")");
    model.onCommand(plain.command);
    CHECK(model.title() == "Alert" && model.message() == "Smoke" && !model.soundEnabled());

    model.show();
    model.begin(DeviceConfiguration());
    CHECK(model.isAcknowledged() && model.message().length() == 0);
    std::printf("AlertModel ok\n");
}
