#include "view_test.h"

#include <limits>

using namespace riot2::views;

int main() {
    auto core = TimerModel::buildTemplate("RIoT2.Ard.M5Core2.Node.TimerView", true);
    CHECK(core.name == "Timer View" && core.classFullName == "RIoT2.Ard.M5Core2.Node.TimerView");
    CHECK(core.deviceParameters.size() == 4 && core.deviceParameters[3].first == "beepOnComplete" &&
          core.deviceParameters[3].second == "false");
    CHECK(core.commandTemplates.size() == 1 && core.reportTemplates.size() == 1);
    CHECK(core.commandTemplates[0].name == "Timer" && core.commandTemplates[0].address == "timer-1");
    CHECK(core.reportTemplates[0].name == "Timer" && core.reportTemplates[0].address == "timer-1");
    CHECK(core.commandTemplates[0].type == riot2::ValueType::Number && core.reportTemplates[0].type == riot2::ValueType::Number);
    auto dial = TimerModel::buildTemplate("RIoT2.Ard.M5Dial.Node.TimerView", false);
    CHECK(dial.classFullName == "RIoT2.Ard.M5Dial.Node.TimerView" && dial.deviceParameters.size() == 3);
    CHECK(dial.deviceParameters[0].first == "defaultMinutes" && dial.deviceParameters[0].second == "5");
    CHECK(dial.deviceParameters[1].first == "stepMinutes" && dial.deviceParameters[2].first == "maxMinutes" &&
          dial.deviceParameters[2].second == "60");

    TimerModel model;
    model.begin(dial);
    CHECK(model.name() == "Timer View" && model.minutes() == 5 && model.stepMinutes() == 1 && model.maxMinutes() == 60);
    CHECK(!model.beepOnComplete() && model.phase() == TimerModel::Phase::Setting && model.remainingSeconds() == 0);

    // Minutes stay within [step, max]; bad parameters fall back to a usable range.
    model.begin(withParameters(dial, {{"defaultMinutes", "7"}, {"stepMinutes", "5"}, {"maxMinutes", "12"}}));
    CHECK(model.minutes() == 7 && model.stepMinutes() == 5 && model.maxMinutes() == 12);
    model.adjust(1);
    CHECK(model.minutes() == 12);
    model.adjust(-1);
    model.adjust(-1);
    CHECK(model.minutes() == 5);
    model.begin(withParameters(dial, {{"defaultMinutes", "0"}, {"stepMinutes", "0"}, {"maxMinutes", "-3"}}));
    CHECK(model.stepMinutes() == 1 && model.maxMinutes() == 1 && model.minutes() == 1);
    model.begin(withParameters(dial, {{"defaultMinutes", "99"}, {"beepOnComplete", "TRUE"}}));
    CHECK(model.minutes() == 60 && model.beepOnComplete());
    model.begin(withParameters(dial, {{"beepOnComplete", "yes"}}));
    CHECK(!model.beepOnComplete());

    // Commands preset the minutes only while in Setting.
    model.begin(withParameters(dial, {{"defaultMinutes", "1"}, {"maxMinutes", "5"}}));
    const char* commandId = dial.commandTemplates[0].id.c_str();
    TestCommand three(commandId, "3"), zero(commandId, "0"), many(commandId, "99"), other("other", "2");
    CHECK(model.onCommand(three.command) && model.minutes() == 3);
    CHECK(model.onCommand(zero.command) && model.minutes() == 1);
    CHECK(model.onCommand(many.command) && model.minutes() == 5);
    CHECK(!model.onCommand(other.command) && model.minutes() == 5);
    model.onCommand(three.command);

    // Running: counts down on millis(), cannot be preset, adjusted, started or dismissed.
    hostMillis = 1000;
    CHECK(!model.cancel() && !model.dismiss() && !model.finishIfElapsed());
    CHECK(model.start() && model.phase() == TimerModel::Phase::Running);
    CHECK(model.totalSeconds() == 180 && model.remainingSeconds() == 180 && model.remainingText() == "03:00");
    CHECK(!model.start() && !model.dismiss());
    CHECK(!model.onCommand(zero.command) && model.minutes() == 3);
    model.adjust(1);
    CHECK(model.minutes() == 3);
    hostMillis += 61500;
    CHECK(model.remainingSeconds() == 119 && model.remainingText() == "01:59" && !model.finishIfElapsed());
    CHECK(model.cancel() && model.phase() == TimerModel::Phase::Setting && model.remainingSeconds() == 0);
    hostMillis += 600000;
    CHECK(!model.finishIfElapsed());

    // Completion exactly once, with one report; no ring without beepOnComplete.
    CHECK(model.start());
    hostMillis += 179999;
    CHECK(!model.finishIfElapsed() && model.remainingText() == "00:01");
    hostMillis += 1;
    CHECK(model.finishIfElapsed() && model.phase() == TimerModel::Phase::Done && !model.finishIfElapsed());
    CHECK(model.remainingSeconds() == 0 && model.remainingText() == "00:00");
    Report report;
    CHECK(model.completionReport(report) && report.id == dial.reportTemplates[0].id && report.value == "0");
    CHECK(model.ringsRemaining() == 0 && !model.ringDue() && !model.takeRing());
    CHECK(!model.onCommand(three.command) && !model.start() && !model.cancel());
    CHECK(model.dismiss() && model.phase() == TimerModel::Phase::Setting);

    // beepOnComplete: kRingCount rings, the first at once, then kRingIntervalMs apart.
    model.begin(withParameters(dial, {{"defaultMinutes", "1"}, {"beepOnComplete", "true"}}));
    hostMillis = std::numeric_limits<unsigned long>::max() - 60099;  // straddles the millis() rollover
    CHECK(model.start());
    hostMillis += 60000;
    CHECK(model.finishIfElapsed() && model.ringsRemaining() == TimerModel::kRingCount);
    CHECK(model.ringDue() && !model.ringDue());
    for (int ring = 1; ring < TimerModel::kRingCount; ++ring) {
        hostMillis += TimerModel::kRingIntervalMs - 1;
        CHECK(!model.ringDue());
        hostMillis += 1;
        CHECK(model.ringDue());
    }
    hostMillis += 10000;
    CHECK(model.ringsRemaining() == 0 && !model.ringDue());

    // Dismissing, restarting or reconfiguring stops a ring in progress.
    CHECK(model.dismiss() && model.start());
    hostMillis += 60000;
    CHECK(model.finishIfElapsed() && model.takeRing() && model.ringsRemaining() == TimerModel::kRingCount - 1);
    CHECK(model.dismiss() && model.ringsRemaining() == 0 && !model.takeRing());
    CHECK(model.start());
    hostMillis += 60000;
    CHECK(model.finishIfElapsed() && model.ringsRemaining() == TimerModel::kRingCount);
    model.begin(dial);
    CHECK(model.ringsRemaining() == 0 && model.phase() == TimerModel::Phase::Setting && !model.ringDue());

    // Reconfiguring a running countdown drops it without completing.
    CHECK(model.start());
    model.begin(dial);
    hostMillis += 600000;
    CHECK(!model.finishIfElapsed() && model.phase() == TimerModel::Phase::Setting);

    // Without templates: no command matches, completion still happens but has no report.
    model.begin(withParameters(DeviceConfiguration(), {{"defaultMinutes", "1"}}));
    CHECK(!model.onCommand(other.command));
    CHECK(model.start());
    hostMillis += 60000;
    CHECK(model.finishIfElapsed() && !model.completionReport(report));
    std::printf("TimerModel ok\n");
}
