#include <riot2/OtaUpdater.h>

#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

bool OtaUpdater::performUpdate(const String& url) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] Wi-Fi not connected, aborting update");
        return false;
    }

    Serial.printf("[OTA] Starting update from %s\n", url.c_str());

    httpUpdate.rebootOnUpdate(true);  // reboots automatically on HTTP_UPDATE_OK

    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    HTTPUpdateResult result;
    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        result = httpUpdate.update(secureClient, url);
    } else {
        result = httpUpdate.update(plainClient, url);
    }

    switch (result) {
        case HTTP_UPDATE_OK:
            // ESP.restart() already happened inside httpUpdate.update(); unreachable.
            return true;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] Server reported no update available");
            return false;
        case HTTP_UPDATE_FAILED:
        default:
            Serial.printf("[OTA] Update failed (%d): %s\n", httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            return false;
    }
}
