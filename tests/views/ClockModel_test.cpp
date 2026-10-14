#include "view_test.h"

using namespace riot2::views;

int main() {
    for (const char* board : {"RIoT2.Ard.M5Core2.Node.ClockView", "RIoT2.Ard.M5Dial.Node.ClockView"}) {
        auto config = ClockModel::buildTemplate(board);
        CHECK(config.name == "Clock View" && config.classFullName == board && config.id.length() > 0);
        CHECK(config.commandTemplates.empty() && config.reportTemplates.empty());
        CHECK(config.deviceParameters.size() == 1 && config.deviceParameters[0].first == "timezone" &&
              config.deviceParameters[0].second == "EET-2EEST,M3.5.0/3,M10.5.0/4");
    }

    ClockModel model;
    model.begin(withParameters(DeviceConfiguration(), {{"timezone", "UTC0"}}));
    CHECK(model.timeZone() == "UTC0");

    String timeText, dateText;
    // 2020-01-01 01:02:03 UTC.
    CHECK(ClockModel::format(ClockModel::kMinValidEpoch + 3723, timeText, dateText));
    CHECK(timeText == "01:02:03" && dateText == "2020-01-01");

    // Before SNTP has synced.
    CHECK(!ClockModel::format(ClockModel::kMinValidEpoch - 1, timeText, dateText));
    CHECK(timeText == "--:--:--" && dateText == "waiting for time sync");
    CHECK(!ClockModel::format(0, timeText, dateText));

    model.begin(withParameters(DeviceConfiguration(), {{"timezone", "EET-2"}}));
    CHECK(ClockModel::format(ClockModel::kMinValidEpoch + 22 * 3600 + 59, timeText, dateText));
    CHECK(timeText == "00:00:59" && dateText == "2020-01-02");

    // Without the parameter the timezone in effect is left alone.
    model.begin(DeviceConfiguration());
    CHECK(model.timeZone().length() == 0);
    CHECK(ClockModel::format(ClockModel::kMinValidEpoch, timeText, dateText));
    CHECK(timeText == "02:00:00" && dateText == "2020-01-01");
    std::printf("ClockModel ok\n");
}
