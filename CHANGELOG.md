# Changelog

All notable documentation changes to `RIoT2.Ard.Shared`.

## [Unreleased]

- Added `WiegandI2CPeripheral` ([I42](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-3-4.md#i42-wiegand-peripheral-s), plan M5 step 4), released with M5Core2 and M5Dial `0.6.0`:
  it reads the RIoT2.Ard.WiegandI2C module over I2C on Grove PORT.A and publishes each card code as
  a text report, with an optional IRQ pin and the formats `hex`, `decimal` and `facility`. Test:
  `tests/wiegand_peripheral.cpp` (in `test_firmware_p2.py`) answers the peripheral's reads with the
  module's production sketch on both boards' pin maps.
- Build: `renovate.json` extends the shared Renovate preset ([I33](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-2.md#i33-supply-chain-s)). Renovate opens grouped
  update pull requests once the app is installed (MA7).
- Added command results and the configuration status ([I19](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-2.md#i19-firmware-results-and-desired-state-m), designs 7.1 phase 3 and 7.2 phase 5),
  released with M5Core2 and M5Dial `0.5.0`:
  - `NodeRuntime` advertises `command-result/1` and `desired-state/1`, answers every command with
    a `correlationId` (`executed`, `unknownCommand`, `rejected`, `failed`, `duplicate`) from a
    16-entry `CorrelationRing` (`CommandResults.h`), and publishes the retained `NodeStatus`
    (`NodeStatus.h`) on every connect, before the presence, and on every change.
  - A configuration command for the applied hash republishes the status instead of fetching; a
    repeat of the hash being fetched keeps the retry backoff (`ConfigurationRetry::pending()`,
    `cancel()`). `OrchestratorClient` sends `If-None-Match`, treats `304` as unchanged, parses
    `revision` and `hash`, and reports `lastError()`.
  - MQTT subscriptions are QoS 1. `MqttConnection` has `publishJson`, `maxPayloadBytes`,
    `onConnected` and `setCapabilities`.
  - `system.ota` restarts after reporting: `OtaUpdater::performUpdate` takes an `onInstalled`
    callback, and a correlated update's id is kept in NVS (`otaCorr`) across the restart.
  - `NodeRuntimeHooks::onCommand` returns whether a view had the command id (breaking for boards).
  - Divergence D5 is fixed: the configuration is fetched from `api/nodes/…`.
  - An empty configuration response no longer counts as unchanged.
- Added `tests/test_desired_state.py` and `tests/runtime_harness.h`; the golden test now also
  checks the hashed configuration command and configuration, the correlated command, the results
  and the status. The host `String` has `substring` and `write`.

- Added `riot2::NodeRuntime` ([I18](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-2.md#i18-firmware-noderuntime-m), plan M5 step 3, ADR 0011): the node lifecycle the
  M5Core2 and M5Dial `main.cpp` files duplicated (NVS settings, provisioning portal, Wi-Fi, MQTT and
  the orchestrator handshake, the deferred configuration fetch with retry, the template server,
  SNTP, `system.ota` and the OTA trial, Grove peripherals and the BLE start-up). Boards react through
  `NodeRuntimeHooks`.
- The golden-message test runs `NodeRuntime`'s handlers once instead of each board's; the
  reconnect, configuration-retry and OTA-rollback tests check `NodeRuntime`, and the GPIO test reads
  the boards' `BoardPeripherals.cpp`.

- Added the view models ([I17](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-2.md#i17-firmware-view-models-m), plan M5 steps 1–2): `riot2::views`
  `AlertModel`, `BleModel`, `ButtonModel`, `ClockModel`, `ColorSchemeModel`, `NotificationModel`,
  `PercentageModel`, `SceneSelectorModel`, `SliderModel`, `TimerModel`, `ToggleModel` and
  `ValueModel`, with `ViewSupport` and `AlertContent` helpers. They hold what the M5Core2 and
  M5Dial views had in common: configuration and parameters, template pairing, command values,
  state and timing, reports and the default templates. The boards' views only render them.
- Added `tests/test_view_models.py` with a test per model (`tests/views`), run by CI, and the
  `tests/host/Arduino.h` shim. The Button, BLE and Timer regressions run the real models.
  `RIOT2_HOST_BUILD` sets the native-test build folder.

- Added `Topics::commandResult()` and `Topics::status()`, mirroring the topics that RIoT2.Core 1.6.0
  reserves ([I14](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-2.md#i14-additive-contract-release-s)). Nothing publishes on them yet.

- Added `OtaRollback` ([I13](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-0-1.md#i13-firmware-ota-rollback-s), backlog item 6): it overrides `verifyRollbackLater()`, so a
  new OTA image starts on trial. `markHealthy()` marks it valid; otherwise an `esp_timer` rolls it
  back after `RIOT2_OTA_VERIFY_TIMEOUT_MS` (default 180000), even when `loop()` hangs. A crash or
  restart during the trial rolls back too. `OtaUpdater` refuses a new update while an image is on
  trial. The build flag `RIOT2_OTA_TEST_NEVER_HEALTHY` makes a test image that always rolls back.
- Changed (divergence C2 fixed): `CommandTemplate::type` and `ReportTemplate::type` are `int`
  (`riot2::ValueType`), parsed from and served as a number, as RIoT2.Core sends them.
  `CommandTemplate::valueType` is removed; the template server no longer sends it. Consumers set
  `type = riot2::ValueType::Number` and so on instead of `"2"`.
- The golden test checks the numeric `type` of commands and reports, and the template server's
  output.

- Added `tests/test_golden_messages.py` ([I05](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/increments/phase-0-1.md#i05-golden-files-and-route-contract-m)):
  the M5Core2 and M5Dial command and configuration handlers and `OrchestratorClient::parseConfiguration`
  read the golden files from RIoT2.Core, and the result is compared with Python's reading of the
  same files. It pins divergence C2 (template `type` and `valueType`). CI clones RIoT2.Core.
- Fixed: `WifiConnection` and `MqttConnection` define their `static constexpr` backoff constants
  out of class, as C++14 requires when `min()` binds them by reference. GCC and Clang host builds
  of the native tests failed to link without them.
- Added CI: every push builds M5Core2, M5Dial and WiegandI2C against this checkout and runs the
  native tests with `g++`.
- Documentation: `AGENTS.md` is the AI instruction file, `CLAUDE.md` imports it, and no version
  notes were present in the README.

## Earlier versions

See git commits; there are no release notes or git tags for earlier versions.
