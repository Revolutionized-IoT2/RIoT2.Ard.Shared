# RIoT2.Ard.Shared

Shared PlatformIO/Arduino C++ library for the ESP32 firmware nodes in the
[RIoT2](https://github.com/Revolutionized-IoT2) platform. It provides Wi-Fi,
provisioning, MQTT, node configuration, OTA, BLE scanning and peripheral plumbing for:

- [RIoT2.Ard.M5Core2.Node](https://github.com/Revolutionized-IoT2/RIoT2.Ard.M5Core2.Node)
- [RIoT2.Ard.M5Dial.Node](https://github.com/Revolutionized-IoT2/RIoT2.Ard.M5Dial.Node)

This repository is not a standalone firmware image. It is consumed by path from each board
project's `platformio.ini`.

## Contents

| Path | Contents |
|---|---|
| `RIoT2Shared/include/riot2/` | Shared public headers for Wi-Fi, MQTT, provisioning, configuration, OTA, BLE, GPIO and Wiegand peripherals and wire models |
| `RIoT2Shared/include/riot2/views/`, `src/views/` | View models: the display-independent logic of the twelve views both boards have (Alert, BLE, Button, Clock, ColorScheme, Notification, Percentage, SceneSelector, Slider, Timer, Toggle, Value) and their default templates |
| `RIoT2Shared/src/` | Implementations compiled into each consuming PlatformIO project |
| `scripts/generate_manifest.py` | PlatformIO pre-build script that generates each board's embedded manifest |
| `tests/` | Native host regression tests and fixtures |

The shared MQTT, HTTP and configuration behavior is specified in the platform contracts:

- [MQTT topics and payloads](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/mqtt-topics.md)
- [Node configuration](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/configuration.md)
- [HTTP API](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/http-api.md)
- [Firmware settings in NVS](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/contracts/env-vars.md#firmware-m5core2-m5dial)

## Using this library from firmware

A consuming PlatformIO project references this repository as a sibling directory:

```ini
lib_extra_dirs = ../RIoT2.Ard.Shared
extra_scripts = pre:../RIoT2.Ard.Shared/scripts/generate_manifest.py
```

Include headers with the `riot2/` prefix:

```cpp
#include <riot2/MqttConnection.h>
#include <riot2/NodeConfig.h>
```

Each consuming project also provides `MANIFEST_NAME` and `VERSION`; `scripts/generate_manifest.py`
uses those files to generate the embedded firmware manifest and a `manifest.json` build artifact.

## Build and test

Run commands from the workspace root (`C:\Src\RIoT2`) in PowerShell:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run -d .\RIoT2.Ard.M5Core2.Node
if ($?) { & $pio run -d .\RIoT2.Ard.M5Dial.Node }
```

Run the host regressions from this repository root when Python and a native C++14 compiler are
available:

```powershell
python .\tests\test_firmware_p1.py
python .\tests\test_firmware_p2.py
python .\tests\test_firmware_architecture.py
python .\tests\test_golden_messages.py
python .\tests\test_desired_state.py
python .\tests\test_view_models.py
```

`test_golden_messages.py` runs both boards' MQTT handlers and `OrchestratorClient::parseConfiguration`
on the [golden files](https://github.com/Revolutionized-IoT2/RIoT2.Core/blob/main/contracts/golden/README.md), checks that the template server writes the same template shape, and checks the command results and the status against the golden ones; RIoT2.Core must be checked out next to this repository. `test_desired_state.py` runs `NodeRuntime`'s
handlers on `tests/runtime_harness.h` (shared with the golden test): the firmware rows of the
desired-state compatibility matrix, the command result rules and the status packet budget. The MQTT regression uses ArduinoJson headers already restored under a board project's
`.pio\libdeps` directory. It does not download dependencies. `test_view_models.py` compiles every
model in `RIoT2Shared/src/views` with each `tests/views/*_test.cpp`, against the `Arduino.h` shim in
`tests/host` (a model without a test fails it). Generated native-test files are kept under
`tests\.host-build` (or `RIOT2_HOST_BUILD`). On Windows, run the tests from a Visual Studio developer shell (they use
`cl`), or set `CXX` to another C++14 compiler.

## Firmware behavior supplied here

`riot2::NodeRuntime` (`NodeRuntime.h`) is the node lifecycle both firmwares run: a board's
`main.cpp` calls `NodeRuntime::startOtaTrial()` first in `setup()`, sets up its hardware, calls
`begin(manifestJson, hooks)` and then `loop()`, and reacts through `NodeRuntimeHooks` (its views'
templates, applying a configuration, commands, OTA and BLE events). Everything below happens in the
runtime:

- Settings are saved in ESP32 NVS namespace `riot2node` by the provisioning portal (`GET /`,
  `POST /save`).
- MQTT topics are centralized in `RIoT2Shared/include/riot2/Topics.h`, mirroring
  `RIoT2.Core/src/Contracts/Constants.cs`.
- Firmware publishes retained online/offline messages and retained last wills through
  PubSubClient, which publishes at QoS 0 only. It subscribes to its command, configuration and
  `riot2/orchestrator/online` topics at QoS 1. The online message advertises the capabilities
  `command-result/1` and `desired-state/1`.
- A command with a `correlationId` is answered on `riot2/node/{id}/command/result` (`CommandResults.h`):
  `executed` when a view (the `onCommand` hook returns true) or peripheral has its id,
  `unknownCommand` when none has, `rejected` before the first configuration or while one is fetched,
  `failed` for a failed `system.ota`. The last 16 outcomes answer redeliveries (`duplicate` for an
  executed one) without running them again.
- The retained configuration status (`NodeStatus.h`) goes to `riot2/node/{id}/status` on every
  connect, before the presence, and on every change: `applying` until the first configuration,
  `applied` with its revision, hash and up to four devices whose class the build lacks, or `failed`
  with the reason.
- Configuration messages contain `apiBaseUrl`, and from Orchestrator 1.5.0 `revision` and `hash`.
  A message for the applied hash only republishes the status. Otherwise firmware fetches
  `{apiBaseUrl}api/nodes/{id}/configuration` with `If-None-Match` (a `304` counts as unchanged).
  Responses larger than 32 KiB are rejected, and failed fetches retry after 1, 2, 4, 8, 16, then
  30 seconds.
- The reserved command id `system.ota` downloads and flashes a firmware `.bin` URL. A correlated
  one is answered `executed` when the image is installed, before the restart, and its id is kept in
  NVS (`otaCorr`), so its redelivery after the restart is `duplicate`.
- `OtaRollback` rolls a new OTA image back to the previous one unless it fetches its configuration
  within 3 minutes (`RIOT2_OTA_VERIFY_TIMEOUT_MS`), or if it crashes or restarts first.
  `NodeRuntime::startOtaTrial()` calls `OtaRollback::begin()`, and the runtime calls
  `OtaRollback::markHealthy()` after a successful configuration fetch. While an image is on trial,
  `system.ota` is refused.
- Grove peripherals registered with `PeripheralFactory` get every command and configuration; the
  BLE scanner starts once a configuration has a BLE-consuming view, with Wi-Fi modem sleep on.
- `WiegandI2CPeripheral` ({I42}) reads a
  [RIoT2.Ard.WiegandI2C](https://github.com/Revolutionized-IoT2/RIoT2.Ard.WiegandI2C) module on
  Grove PORT.A (A1 SDA, A2 SCL, I2C `0x26`) and publishes every card code as a text report.
  Parameters: `address`, `irq` (`B1` or `B2`: read only while the module's IRQ is high, and report
  the code 0 too; empty: poll every 100 ms) and `format` (`hex` `12AB3F`, `decimal` `1223487`,
  `facility` `18:43839`). Both boards register it.
- Command and report templates carry `type`, RIoT2.Core's `ValueType`, as a number
  (`riot2::ValueType`), both when parsed from the orchestrator and when served to it.
- `ConfigTemplateServer` serves `GET /api/device/configuration/templates` so the orchestrator can
  discover the board's view and peripheral templates.

## Contributing

- Instructions for AI coding agents: [AGENTS.md](AGENTS.md).
- Release notes: [CHANGELOG.md](CHANGELOG.md).
- Platform documentation hub: [.github/docs](https://github.com/Revolutionized-IoT2/.github/blob/main/docs/README.md).

Reusable firmware logic belongs here, not in only one board repository. A change to this library is
done only after both M5Core2 and M5Dial firmware builds pass.

## License

See [LICENSE](LICENSE).
