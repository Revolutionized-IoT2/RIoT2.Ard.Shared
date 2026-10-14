#pragma once

#include <Arduino.h>

// Milliseconds a new OTA image has to connect and fetch its configuration before it is rolled back.
#ifndef RIOT2_OTA_VERIFY_TIMEOUT_MS
#define RIOT2_OTA_VERIFY_TIMEOUT_MS 180000UL
#endif

// Automatic rollback of a bad OTA image (backlog item 6), on top of ESP-IDF app rollback, which
// the Arduino core's bootloader has enabled. Linking this module stops initArduino() from marking
// every image valid at boot (it overrides verifyRollbackLater()). The firmware must:
//
//   setup():  call OtaRollback::begin() first.
//   After the first successful configuration fetch: call OtaRollback::markHealthy().
//
// A new image that doesn't get that far within RIOT2_OTA_VERIFY_TIMEOUT_MS is marked invalid and
// the device restarts into the previous image. The deadline is an esp_timer, so it also fires when
// setup() or loop() hangs. A crash or restart before markHealthy() rolls back as well: the
// bootloader never starts an unconfirmed image twice. Images flashed by cable are not on trial.
namespace OtaRollback {

void begin();
void markHealthy();

// True while a new image is on trial. OtaUpdater then refuses a further update, which would
// overwrite the previous image that a rollback returns to.
bool isPendingVerify();

}  // namespace OtaRollback
