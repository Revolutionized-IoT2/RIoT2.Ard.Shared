#include <riot2/OtaRollback.h>

#include <atomic>

#include <esp_ota_ops.h>
#include <esp_timer.h>

// The Arduino core's weak default returns false, and initArduino() then marks every image valid
// before setup() runs. Returning true leaves the decision to OtaRollback.
extern "C" bool verifyRollbackLater() { return true; }

namespace {

// True while a new image is on trial. Whoever sets it to false (markHealthy() on the main task or
// the deadline on the esp_timer task) decides, and the other one then does nothing.
std::atomic<bool> onTrial{false};
esp_timer_handle_t deadline = nullptr;

bool endTrial() { return onTrial.exchange(false); }

void rollBack(void*) {
    if (!endTrial()) {
        return;
    }
    Serial.println("[OTA] New image did not become healthy in time, rolling back to the previous image");
    Serial.flush();
    // Restarts into the previous image; returns only if there is none. Then this image is all
    // there is: keep it, so that a later OTA update isn't refused.
    esp_err_t result = esp_ota_mark_app_invalid_rollback_and_reboot();
    Serial.printf("[OTA] Rollback failed (%s), keeping this image\n", esp_err_to_name(result));
    esp_ota_mark_app_valid_cancel_rollback();
}

}  // namespace

void OtaRollback::begin() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running == nullptr || esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }

    onTrial = true;
    esp_timer_create_args_t args = {};
    args.callback = rollBack;
    args.name = "ota-rollback";
    if (esp_timer_create(&args, &deadline) != ESP_OK ||
        esp_timer_start_once(deadline, static_cast<uint64_t>(RIOT2_OTA_VERIFY_TIMEOUT_MS) * 1000ULL) != ESP_OK) {
        // Without a deadline only a restart rolls back; better than marking a bad image valid.
        Serial.println("[OTA] Could not start the rollback timer");
    }
    Serial.printf("[OTA] New image in %s is on trial: it rolls back unless it fetches its configuration within %lu s\n",
                  running->label, static_cast<unsigned long>(RIOT2_OTA_VERIFY_TIMEOUT_MS / 1000));
}

void OtaRollback::markHealthy() {
#ifdef RIOT2_OTA_TEST_NEVER_HEALTHY
    // Test build for the rollback procedure in the board READMEs: this image never earns its place.
    if (onTrial) {
        Serial.println("[OTA] RIOT2_OTA_TEST_NEVER_HEALTHY: not marking this image healthy");
    }
#else
    if (!endTrial()) {
        return;
    }
    if (deadline != nullptr) {
        esp_timer_stop(deadline);
    }
    esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("[OTA] New image is healthy and marked valid (%s)\n", esp_err_to_name(result));
#endif
}

bool OtaRollback::isPendingVerify() { return onTrial; }
