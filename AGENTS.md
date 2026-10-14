# AGENTS.md — RIoT2.Ard.Shared

Applies to: this repository. Read the platform guide first:
[.github/AGENTS.md](https://github.com/Revolutionized-IoT2/.github/blob/main/AGENTS.md). It covers the
workspace map, platform-wide rules and the documentation rules. In the local workspace, every
`https://github.com/Revolutionized-IoT2/<Repo>/blob/main/<path>` link is the file
`C:\Src\RIoT2\<Repo>\<path>`; read the local file instead of fetching the URL.

## What this is

Shared PlatformIO/Arduino C++ code for the RIoT2 ESP32 firmware nodes. It owns Wi-Fi,
provisioning, MQTT, HTTP configuration fetch, OTA, BLE scanning, GPIO peripherals, topic mirrors
and firmware wire models. It is consumed by path by M5Core2 and M5Dial; it is not flashed by
itself.

## Commands

Run from the workspace root (`C:\Src\RIoT2`), in PowerShell:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run -d .\RIoT2.Ard.M5Core2.Node
if ($?) { & $pio run -d .\RIoT2.Ard.M5Dial.Node }
```

Run native host regressions from this repository root (`C:\Src\RIoT2\RIoT2.Ard.Shared`) when a
C++14 compiler is available:

```powershell
python .\tests\test_firmware_p1.py
python .\tests\test_firmware_p2.py
python .\tests\test_firmware_architecture.py
python .\tests\test_golden_messages.py
python .\tests\test_view_models.py
```

- Build: build both consumers. This repository has no standalone PlatformIO environment.
- Test: the Python tests compile selected production code with host fakes; the MQTT and golden
  tests need ArduinoJson headers already present under a board's `.pio\libdeps`. The golden test
  reads `../RIoT2.Core/contracts/golden` (CI clones RIoT2.Core) and pins the numeric template
  `type` in the parser and in `DeviceConfigurationJson.cpp`.
- Run: no local service or device run command exists here.
- CI: `.github/workflows/ci.yml` builds M5Core2, M5Dial and WiegandI2C against this checkout and
  runs the native tests with `g++` on every push ([CI guide](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/guides/ci.md)). The tests must also pass with
  GCC and Clang, so define `static constexpr` members that are bound by reference (C++14).
- Release: none. Consumers ship firmware images that include this source by path.

## Layout

| Path | Contents |
|---|---|
| `RIoT2Shared/include/riot2/Topics.h` | Firmware copy of RIoT2 MQTT topic templates |
| `RIoT2Shared/include/riot2/DeviceConfiguration.h` | Firmware subset of node configuration and templates |
| `RIoT2Shared/include/riot2/NodeConfig.h`, `src/NodeConfig.cpp` | NVS-backed node id, Wi-Fi, MQTT and feedback settings |
| `RIoT2Shared/include/riot2/NodeRuntime.h`, `src/NodeRuntime.cpp` | The node lifecycle both boards run: NVS settings, provisioning, Wi-Fi, MQTT handshake, configuration fetch and retry, templates, SNTP, OTA, peripherals, BLE start |
| `RIoT2Shared/include/riot2/MqttConnection.h`, `src/MqttConnection.cpp` | PubSubClient lifecycle, online/LWT messages, command/report routing |
| `RIoT2Shared/include/riot2/OrchestratorClient.h`, `src/OrchestratorClient.cpp` | Blocking configuration fetch and parser |
| `RIoT2Shared/include/riot2/ProvisioningPortal.h`, `src/ProvisioningPortal.cpp` | SoftAP captive portal (`GET /`, `POST /save`) |
| `RIoT2Shared/include/riot2/ConfigTemplateServer.h`, `src/ConfigTemplateServer.cpp` | `GET /api/device/configuration/templates` |
| `RIoT2Shared/include/riot2/OtaUpdater.h`, `src/OtaUpdater.cpp` | `system.ota` firmware download and flash |
| `RIoT2Shared/include/riot2/OtaRollback.h`, `src/OtaRollback.cpp` | Trial of a new OTA image: marks it valid after the first configuration fetch, or rolls it back on a deadline |
| `RIoT2Shared/include/riot2/BleScanner.h`, `src/BleScanner.cpp` | Shared BLE scanner and callbacks |
| `RIoT2Shared/include/riot2/GpioPeripheral.h`, `src/GpioPeripheral.cpp` | Shared Grove GPIO peripheral |
| `RIoT2Shared/include/riot2/views/`, `src/views/` | `riot2::views` models of the shared view types, `ViewSupport` helpers (slot pairing, command values, report literals, templates) |
| `scripts/generate_manifest.py` | PlatformIO pre-build manifest generation |
| `tests/` | Host regression harness and fixtures; `tests/views/*_test.cpp` test the view models, `tests/host/Arduino.h` is their `Arduino.h` |

## Contracts implemented here

- [mqtt-topics.md](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/mqtt-topics.md):
  `Topics.h`, `MqttConnection.cpp`, `Report.h`, `Command.h`.
- [configuration.md](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/configuration.md):
  `DeviceConfiguration.h`, `DeviceConfigurationJson.cpp`, `OrchestratorClient.cpp`.
- [http-api.md](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/http-api.md):
  `OrchestratorClient.cpp`, `ConfigTemplateServer.cpp`, `ProvisioningPortal.cpp`.
- [env-vars.md firmware section](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/env-vars.md#firmware-m5core2-m5dial):
  `NodeConfig.cpp` NVS namespace and keys.

## Rules

- Put reusable firmware logic here, not in only one board repository. A view's logic (configuration
  and parameters, template pairing, command values, state, timing, reports, default template) goes
  in its `riot2::views` model; the board views only render the model and turn input into model
  calls. A new model needs `tests/views/<Model>_test.cpp`.
- View models build on the host: use only what `tests/host/Arduino.h` provides (extend it when you
  need more), `millis()`, ArduinoJson and the standard library; never `JsonVariant::as<String>()`
  (use `commandText`) or board, LVGL, M5 or sound APIs.
- A change here is done only when both `RIoT2.Ard.M5Core2.Node` and `RIoT2.Ard.M5Dial.Node` build.
- Keep firmware MQTT topics mirrored from `RIoT2.Core/Constants.cs`; update hub contracts when a
  cross-repository contract changes.
- Do not assign `system.ota` to a view command template; it is reserved for firmware OTA.
- Lifecycle code (Wi-Fi, MQTT, configuration, OTA, peripherals, BLE start) goes in `NodeRuntime`;
  a board's `main.cpp` keeps hardware setup, its UI and `NodeRuntimeHooks`. A board feature the
  runtime needs is a hook, not a board `#ifdef`.
- Every firmware calls `NodeRuntime::startOtaTrial()` first in `setup()`; `NodeRuntime` calls
  `OtaRollback::markHealthy()` only after `requestConfiguration()` succeeded. Don't mark an image
  healthy earlier, and don't restart a device on purpose while `OtaRollback::isPendingVerify()`: a
  restart rolls back.
- Template `type` is an `int` (`riot2::ValueType`), as in RIoT2.Core. Don't add a separate
  `valueType`.
- Do not read or edit vendored `.pio\libdeps` content except to use restored headers for tests.
- Do not store real Wi-Fi, MQTT, node ids, broker addresses or certificates in docs or examples.
- Keep provisioning settings aligned with NVS namespace `riot2node` and the hub firmware settings
  table.
- Prefer logged failures over silent fallbacks when parsing URLs, publishing MQTT JSON or fetching
  configuration.

## Pitfalls

- Firmware publishes retained online/offline/LWT messages, as the .NET node does since Core 1.8.0
  (divergence D1, fixed in I16), but delivery is still QoS 0 through PubSubClient.
- `OrchestratorClient` fetches `{apiBaseUrl}api/Nodes/{id}/configuration` with a capital `Nodes`.
  That works but is listed as divergence D5.
- Configuration responses are capped at 32 KiB. Fetch retries are scheduled in the main loop after
  1, 2, 4, 8, 16 and then 30 seconds.
- HTTPS MQTT, configuration and OTA use `RIOT2_ROOT_CA_PEM` when compiled in. Without it the code
  logs a warning and falls back to `setInsecure()`.
- OTA rollback depends on the bootloader and Arduino core of the pinned `espressif32` platform
  (Arduino-ESP32 2.0.17, built with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`). Check both, and that
  `verifyRollbackLater()` still exists, before you change the platform version in a board's
  `platformio.ini`.
- `ConfigTemplateServer` serves firmware templates at `/api/device/configuration/templates`, but
  the current hub firmware HTTP section only mentions provisioning endpoints.

## Related work

- Backlog item 15 (duplicated firmware views and runtime wiring): done in I17 and I18.
- Optional hardening [S9](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/backlog/optional-hardening.md):
  provisioning security, NVS encryption, signed OTA and fail-closed TLS.
- Accepted architecture item ([ADR 0011](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/adr/0011-firmware-noderuntime.md)) [A8](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/architecture/target.md#a8-share-a-firmware-noderuntime):
  shared firmware `NodeRuntime`.
- Plan [M5](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/plans/m05-firmware-node-runtime.md):
  shared firmware view logic and node runtime.
- [Feature ideas](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/features.md#firmware):
  Wiegand peripheral integration and OTA channel improvements.
