#include "riot2/views/ClockModel.h"

#include <stdio.h>
#include <stdlib.h>

namespace riot2 {
namespace views {

constexpr time_t ClockModel::kMinValidEpoch;

void ClockModel::begin(const DeviceConfiguration& config) {
    _timeZone = findParameter(config.deviceParameters, "timezone", "");
    if (_timeZone.length() > 0) {
        applyTimeZone(_timeZone.c_str());
    }
}

// The host tests build with MSVC too, which has neither setenv() nor localtime_r().
void ClockModel::applyTimeZone(const char* timeZone) {
#ifdef _WIN32
    _putenv_s("TZ", timeZone);
    _tzset();
#else
    setenv("TZ", timeZone, 1);
    tzset();
#endif
}

bool ClockModel::toLocalTime(time_t now, struct tm& local) {
#ifdef _WIN32
    return localtime_s(&local, &now) == 0;
#else
    return localtime_r(&now, &local) != nullptr;
#endif
}

bool ClockModel::format(time_t now, String& timeText, String& dateText) {
    struct tm timeInfo;
    if (now < kMinValidEpoch || !toLocalTime(now, timeInfo)) {
        timeText = "--:--:--";
        dateText = "waiting for time sync";
        return false;
    }
    char timeBuf[9];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    timeText = timeBuf;

    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1,
             timeInfo.tm_mday);
    dateText = dateBuf;
    return true;
}

DeviceConfiguration ClockModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Clock View");
    config.deviceParameters = {{"timezone", "EET-2EEST,M3.5.0/3,M10.5.0/4"}};
    return config;
}

}  // namespace views
}  // namespace riot2
