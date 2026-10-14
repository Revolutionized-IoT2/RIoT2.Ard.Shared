#include "view_test.h"

using namespace riot2::views;

int main() {
    for (const char* board : {"RIoT2.Ard.M5Core2.Node.NotificationView", "RIoT2.Ard.M5Dial.Node.NotificationView"}) {
        auto config = NotificationModel::buildTemplate(board);
        CHECK(config.name == "Notification View" && config.classFullName == board && config.id.length() > 0);
        CHECK(config.reportTemplates.empty());
        CHECK(config.deviceParameters.size() == 1 && config.deviceParameters[0].first == "durationMs" &&
              config.deviceParameters[0].second == "4000");
        CHECK(config.commandTemplates.size() == 1 && config.commandTemplates[0].name == "Notification");
        CHECK(config.commandTemplates[0].address == "notice-1" &&
              config.commandTemplates[0].type == riot2::ValueType::Entity);
    }

    NotificationModel model;
    model.begin(DeviceConfiguration());
    CHECK(model.durationMs() == 4000 && model.title() == "Notification" && model.message().length() == 0);
    CHECK(model.subHeader().length() == 0 && !model.soundEnabled() && model.isExpired());

    // durationMs: missing, zero, negative or not a number fall back to 4 seconds.
    for (const char* bad : {"", "0", "-5", "abc"}) {
        model.begin(withParameters(DeviceConfiguration(), {{"durationMs", bad}}));
        CHECK(model.durationMs() == 4000);
    }
    model.begin(withParameters(DeviceConfiguration(), {{"durationMs", "1000"}}));
    CHECK(model.durationMs() == 1000);

    TestCommand status("n", R"({"title":"Washer","message":"Done","subHeader":"Laundry","soundEnabled":"true"})");
    model.onCommand(status.command);
    CHECK(model.title() == "Washer" && model.message() == "Done" && model.subHeader() == "Laundry");
    CHECK(model.soundEnabled() && model.isExpired());

    hostMillis = 5000;
    model.show();
    CHECK(!model.isExpired() && model.remaining() == 1.f);
    hostMillis = 5250;
    CHECK(!model.isExpired() && model.remaining() == 0.75f);
    hostMillis = 5999;
    CHECK(!model.isExpired());
    hostMillis = 6000;
    CHECK(model.isExpired() && model.remaining() == 0.f);
    hostMillis = 9000;
    CHECK(model.isExpired() && model.remaining() == 0.f);

    // A tap dismisses it early; show() starts over.
    model.show();
    CHECK(!model.isExpired());
    model.dismiss();
    CHECK(model.isExpired());

    // Across the millis() wrap.
    hostMillis = static_cast<unsigned long>(-500);
    model.show();
    hostMillis = 400;
    CHECK(!model.isExpired());
    hostMillis = 500;
    CHECK(model.isExpired());

    TestCommand plain("n", R"("Hi")");
    model.onCommand(plain.command);
    CHECK(model.title() == "Notification" && model.message() == "Hi" && model.subHeader().length() == 0);
    std::printf("NotificationModel ok\n");
}
