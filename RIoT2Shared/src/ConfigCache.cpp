#include <riot2/ConfigCache.h>

#include <LittleFS.h>

namespace {
constexpr const char* kPath = "/config.json";
bool mounted = false;
}  // namespace

void ConfigCache::begin() {
    if (mounted) {
        return;
    }
    mounted = LittleFS.begin(true);  // format on first mount if needed
    if (!mounted) {
        Serial.println("[ConfigCache] Failed to mount LittleFS");
    }
}

void ConfigCache::save(const String& rawJson) {
    if (!mounted) {
        return;
    }
    File f = LittleFS.open(kPath, "w");
    if (!f) {
        Serial.println("[ConfigCache] Failed to open config.json for writing");
        return;
    }
    f.print(rawJson);
    f.close();
}

bool ConfigCache::tryLoad(String& out) {
    if (!mounted || !LittleFS.exists(kPath)) {
        return false;
    }
    File f = LittleFS.open(kPath, "r");
    if (!f) {
        return false;
    }
    out = f.readString();
    f.close();
    return out.length() > 0;
}
