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
```

- Build: build both consumers. This repository has no standalone PlatformIO environment.
- Test: the Python tests compile selected production code with host fakes; the MQTT and golden
  tests need ArduinoJson headers already present under a board's `.pio\libdeps`. The golden test
  reads `../RIoT2.Core/contracts/golden` (CI clones RIoT2.Core) and pins divergence C2.
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
| `RIoT2Shared/include/riot2/MqttConnection.h`, `src/MqttConnection.cpp` | PubSubClient lifecycle, online/LWT messages, command/report routing |
| `RIoT2Shared/include/riot2/OrchestratorClient.h`, `src/OrchestratorClient.cpp` | Blocking configuration fetch and parser |
| `RIoT2Shared/include/riot2/ProvisioningPortal.h`, `src/ProvisioningPortal.cpp` | SoftAP captive portal (`GET /`, `POST /save`) |
| `RIoT2Shared/include/riot2/ConfigTemplateServer.h`, `src/ConfigTemplateServer.cpp` | `GET /api/device/configuration/templates` |
| `RIoT2Shared/include/riot2/OtaUpdater.h`, `src/OtaUpdater.cpp` | `system.ota` firmware download and flash |
| `RIoT2Shared/include/riot2/BleScanner.h`, `src/BleScanner.cpp` | Shared BLE scanner and callbacks |
| `RIoT2Shared/include/riot2/GpioPeripheral.h`, `src/GpioPeripheral.cpp` | Shared Grove GPIO peripheral |
| `scripts/generate_manifest.py` | PlatformIO pre-build manifest generation |
| `tests/` | Host regression harness and fixtures |

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

- Put reusable firmware logic here, not in only one board repository.
- A change here is done only when both `RIoT2.Ard.M5Core2.Node` and `RIoT2.Ard.M5Dial.Node` build.
- Keep firmware MQTT topics mirrored from `RIoT2.Core/Constants.cs`; update hub contracts when a
  cross-repository contract changes.
- Do not assign `system.ota` to a view command template; it is reserved for firmware OTA.
- Do not read or edit vendored `.pio\libdeps` content except to use restored headers for tests.
- Do not store real Wi-Fi, MQTT, node ids, broker addresses or certificates in docs or examples.
- Keep provisioning settings aligned with NVS namespace `riot2node` and the hub firmware settings
  table.
- Prefer logged failures over silent fallbacks when parsing URLs, publishing MQTT JSON or fetching
  configuration.

## Pitfalls

- Firmware publishes retained online/offline/LWT messages, but delivery is still QoS 0 through
  PubSubClient. This is divergence D1 in the hub MQTT contract.
- `OrchestratorClient` fetches `{apiBaseUrl}api/Nodes/{id}/configuration` with a capital `Nodes`.
  That works but is listed as divergence D5.
- Configuration responses are capped at 32 KiB. Fetch retries are scheduled in the main loop after
  1, 2, 4, 8, 16 and then 30 seconds.
- Firmware parses template `type` as `String`, while the orchestrator sends a numeric `type` and
  does not send `valueType`; downloaded numeric type information is lost (configuration divergence
  C2).
- HTTPS MQTT, configuration and OTA use `RIOT2_ROOT_CA_PEM` when compiled in. Without it the code
  logs a warning and falls back to `setInsecure()`.
- `ConfigTemplateServer` serves firmware templates at `/api/device/configuration/templates`, but
  the current hub firmware HTTP section only mentions provisioning endpoints.

## Related work

- Backlog item [6](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/backlog/open-issues.md):
  OTA rollback policy.
- Backlog item [15](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/backlog/open-issues.md):
  duplicated firmware views and runtime wiring.
- Optional hardening [S9](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/backlog/optional-hardening.md):
  provisioning security, NVS encryption, signed OTA and fail-closed TLS.
- Accepted architecture item ([ADR 0011](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/adr/0011-firmware-noderuntime.md)) [A8](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/architecture/target.md#a8-share-a-firmware-noderuntime):
  shared firmware `NodeRuntime`.
- Plan [M5](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/plans/m05-firmware-node-runtime.md):
  shared firmware view logic and node runtime.
- [Feature ideas](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/features.md#firmware):
  Wiegand peripheral integration and OTA channel improvements.
