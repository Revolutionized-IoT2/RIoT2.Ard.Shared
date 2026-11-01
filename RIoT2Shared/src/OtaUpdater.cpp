#include <riot2/OtaUpdater.h>

#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <riot2/OtaRollback.h>
#include <riot2/TlsRootCa.h>

bool OtaUpdater::performUpdate(const String& url, const std::function<void()>& onInstalled) {
    if (OtaRollback::isPendingVerify()) {
        // The other slot holds the previous, known-good image: the one a rollback returns to.
        // Writing over it before this image has proved itself would leave nothing to go back to.
        Serial.println("[OTA] This image is still on trial (not yet marked valid), refusing a new update");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] Wi-Fi not connected, aborting update");
        return false;
    }

    Serial.printf("[OTA] Starting update from %s\n", url.c_str());

    // The restart happens below, after onInstalled has reported the result.
    httpUpdate.rebootOnUpdate(false);

    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    HTTPUpdateResult result;
    if (url.startsWith("https://")) {
        const char* pem = riot2::rootCaPem();
        if (pem[0] != '\0') {
            secureClient.setCACert(pem);
        } else {
            Serial.println(
                "[OTA] WARNING: no root CA configured (RIOT2_ROOT_CA_PEM), HTTPS certificate will NOT be "
                "validated");
            secureClient.setInsecure();
        }
        result = httpUpdate.update(secureClient, url);
    } else {
        result = httpUpdate.update(plainClient, url);
    }

    switch (result) {
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Update installed, restarting");
            if (onInstalled) {
                onInstalled();
            }
            delay(100);
            ESP.restart();
            return true;  // unreachable
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
