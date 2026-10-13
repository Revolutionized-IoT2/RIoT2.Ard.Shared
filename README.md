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
| `RIoT2Shared/include/riot2/` | Shared public headers for Wi-Fi, MQTT, provisioning, configuration, OTA, BLE, GPIO peripherals and wire models |
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
```

`test_golden_messages.py` runs both boards' MQTT handlers and `OrchestratorClient::parseConfiguration`
on the [golden files](https://github.com/Revolutionized-IoT2/RIoT2.Core/blob/main/contracts/golden/README.md); RIoT2.Core must be checked out next to this repository. The MQTT regression uses ArduinoJson headers already restored under a board project's
`.pio\libdeps` directory. It does not download dependencies. Generated native-test files are kept
under `tests\.host-build`.

## Firmware behavior supplied here

- Settings are saved in ESP32 NVS namespace `riot2node` by the provisioning portal (`GET /`,
  `POST /save`).
- MQTT topics are centralized in `RIoT2Shared/include/riot2/Topics.h`, mirroring
  `RIoT2.Core/Constants.cs`.
- Firmware publishes retained online/offline messages and retained last wills through
  PubSubClient at QoS 0.
- Configuration messages contain `apiBaseUrl`; firmware fetches
  `{apiBaseUrl}api/Nodes/{id}/configuration`. Responses larger than 32 KiB are rejected, and
  failed fetches retry after 1, 2, 4, 8, 16, then 30 seconds.
- The reserved command id `system.ota` downloads and flashes a firmware `.bin` URL.
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
