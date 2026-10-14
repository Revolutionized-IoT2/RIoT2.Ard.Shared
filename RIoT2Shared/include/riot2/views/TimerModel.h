#pragma once

#include <Arduino.h>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// Countdown egg-timer with three phases: Setting (choose whole minutes) -> Running (counting down
// on millis()) -> Done ("Time's up!" until dismissed). deviceParameters: "defaultMinutes" (default
// 5), "stepMinutes" (default 1), "maxMinutes" (default 60) - minutes stay within [stepMinutes,
// maxMinutes] - and "beepOnComplete" (default false): ring kRingCount times instead of one
// confirm sound on completion.
//
// Bound to the first command and report template. A command presets the minutes, but only while
// in Setting (never interrupts a running countdown). The report {id, "0"} is published exactly
// once per completed countdown, not every second while running. The view drives the countdown
// (finishIfElapsed()) whether or not it is shown; the model never plays sounds itself.
class TimerModel {
public:
    enum class Phase { Setting, Running, Done };

    static constexpr int kRingCount = 6;                  // number of beeps in the "egg timer ring" pattern
    static constexpr unsigned long kRingIntervalMs = 260;  // spacing between those beeps

    // Back to Setting; stops a countdown or ring without a report.
    void begin(const DeviceConfiguration& config);

    const String& name() const { return _name; }
    int minutes() const { return _minutes; }
    int stepMinutes() const { return _stepMinutes; }
    int maxMinutes() const { return _maxMinutes; }
    bool beepOnComplete() const { return _beepOnComplete; }
    Phase phase() const { return _phase; }

    // Length of the current/last countdown.
    long totalSeconds() const { return _totalSeconds; }
    // Seconds left while Running, 0 otherwise.
    long remainingSeconds() const;
    // remainingSeconds() as "MM:SS".
    String remainingText() const;

    // In Setting: one step up (delta > 0) or down, within [stepMinutes, maxMinutes].
    void adjust(int delta);

    // Presets the minutes. Returns false for another command id or outside Setting.
    bool onCommand(const Command& command);

    // Setting -> Running for minutes(). Returns false in another phase.
    bool start();
    // Running -> Setting without a report. Returns false in another phase.
    bool cancel();
    // Done -> Setting, stopping any ring still playing. Returns false in another phase.
    bool dismiss();

    // Running -> Done once the countdown has run out: true exactly once per countdown. With
    // beepOnComplete the ring starts, otherwise the view plays its confirm sound.
    bool finishIfElapsed();
    // The completion report. False without a report template.
    bool completionReport(Report& report) const;

    // Rings of the current ring pattern still to play.
    int ringsRemaining() const { return _ringsRemaining; }
    // For views polling from loop(): true (and counted) when the next ring is due - the first at
    // once, the others kRingIntervalMs apart.
    bool ringDue();
    // For views with their own kRingIntervalMs timer: counts one ring; false when none is left.
    bool takeRing();

    // `beepParameter` adds deviceParameter "beepOnComplete" ("false").
    static DeviceConfiguration buildTemplate(const String& classFullName, bool beepParameter);

private:
    String _name;
    String _commandId;
    String _reportId;
    int _stepMinutes = 1;
    int _maxMinutes = 60;
    int _minutes = 5;
    bool _beepOnComplete = false;

    Phase _phase = Phase::Setting;
    long _totalSeconds = 0;
    unsigned long _startMs = 0;
    int _ringsRemaining = 0;
    unsigned long _lastRingMs = 0;

    int clampMinutes(long minutes) const;
};

}  // namespace views
}  // namespace riot2
