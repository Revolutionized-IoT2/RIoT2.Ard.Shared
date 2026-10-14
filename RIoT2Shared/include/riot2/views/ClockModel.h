#pragma once

#include <Arduino.h>

#include <time.h>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// The current time and date, in the timezone of the optional "timezone" deviceParameter: a POSIX
// TZ string (e.g. "EET-2EEST,M3.5.0/3,M10.5.0/4" for Helsinki, with full DST support) applied via
// setenv("TZ", ...)/tzset() so localtime_r() shows local time instead of UTC. If absent, time is
// shown in UTC (main.cpp's configTime() call has no gmtOffset/DST applied). Takes no
// commandTemplates/reportTemplates.
class ClockModel {
public:
    // time(nullptr) returns small values (seconds since boot-ish) before SNTP
    // has synced; treat anything before 2020-01-01 as "not yet synced".
    static constexpr time_t kMinValidEpoch = 1577836800;

    // Applies the "timezone" deviceParameter process-wide. Left alone (no setenv/tzset call) when
    // unset, preserving UTC-only behavior.
    void begin(const DeviceConfiguration& config);

    const String& timeZone() const { return _timeZone; }

    // `now` as local "HH:MM:SS" and "YYYY-MM-DD". Before time is synced returns false with
    // "--:--:--" and "waiting for time sync".
    static bool format(time_t now, String& timeText, String& dateText);
    static bool now(String& timeText, String& dateText) { return format(::time(nullptr), timeText, dateText); }

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    static void applyTimeZone(const char* timeZone);
    static bool toLocalTime(time_t now, struct tm& local);

    String _timeZone;
};

}  // namespace views
}  // namespace riot2
