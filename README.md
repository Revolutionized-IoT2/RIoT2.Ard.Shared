# RIoT2.Ard.Shared

Shared PlatformIO library providing the Wi-Fi/MQTT/orchestrator/provisioning/BLE/peripheral
framework code common to every RIoT2 Ard node firmware —
[RIoT2.Ard.M5Dial.Node](../RIoT2.Ard.M5Dial.Node) and
[RIoT2.Ard.M5Core2.Node](../RIoT2.Ard.M5Core2.Node) — so the two projects don't drift apart on
protocol/connectivity behavior. It is **not** a standalone buildable/flashable project; it only
builds as a dependency of a consuming node project.

This library intentionally stops at the display: it has no rendering model, no `IView`
interface, and no concrete UI classes — each node project keeps its own UI framework local
(hand-drawn M5GFX for M5Dial, LVGL for Core2), since that's where the two projects genuinely
differ.

## Wiring into a consuming project

A node project's `platformio.ini` references this directory directly — no publishing/versioning
step, no symlink, and no admin/Developer-Mode privilege needed on Windows:

```ini
lib_extra_dirs = ../RIoT2.Ard.Shared
extra_scripts = pre:../RIoT2.Ard.Shared/scripts/generate_manifest.py
```

`lib_extra_dirs` is PlatformIO's documented mechanism for building a library directory shared
across sibling projects. Headers are included with the `riot2/` namespace prefix to make shared
vs. project-local code obvious at the call site:

```cpp
#include <riot2/MqttConnection.h>
#include <riot2/NodeConfig.h>
```

Consuming projects must sit as siblings of this directory (e.g. both under `C:\Src\RIoT2\`) for
the relative path in `lib_extra_dirs` to resolve.

## What's in here

| Component | Purpose |
| --- | --- |
| `NodeConfig.h/.cpp` | NVS-backed (`Preferences`) node identity/credentials struct: `id`, `name`, Wi-Fi/MQTT credentials, `mqttUseTls`, `vibrateEnabled`. |
| `WifiConnection.h/.cpp` | Wi-Fi connect with retry/backoff and status reporting. |
| `MqttConnection.h/.cpp` | MQTT connect/reconnect, LWT, online/offline announcements, command/report pub-sub; switches to `WiFiClientSecure` + port 8883 when `NodeConfig::mqttUseTls` is set. |
| `OrchestratorClient.h/.cpp` | HTTPS fetch of a node's device configuration from the orchestrator, with root-CA pinning (see `TlsRootCa`); also handles caching (see `ConfigCache`) and reconfiguration pushes. |
| `TlsRootCa.h/.cpp` | Root CA (PEM) used to validate both the orchestrator's HTTPS certificate and (when enabled) the MQTT broker's TLS certificate; overridable per deployment via `-DRIOT2_ROOT_CA_PEM=...`. |
| `ConfigCache.h/.cpp` | Persists the last-fetched device configuration JSON to LittleFS so a node can rebuild its UI offline within seconds of power-on, before Wi-Fi/MQTT/orchestrator are even reachable. |
| `ProvisioningPortal.h/.cpp` | First-boot captive-portal web form (Wi-Fi/MQTT/TLS/vibration settings) shown when no valid `NodeConfig` is stored; each project supplies its own display title. |
| `OtaUpdater.h/.cpp` | Downloads and flashes a firmware `.bin` from a URL (triggered by the `system.ota` command id), reboots on success. |
| `BleScanner.h/.cpp`, `BleTypes.h` | Continuous, non-blocking BLE advertisement scan (`NimBLE-Arduino`), fanned out to consumers via callbacks; only started once a project confirms it has a BLE-consuming view. |
| `GpioPeripheral.h/.cpp`, `IPeripheral.h` | Generic digital-I/O peripheral driving up to 4 pins via a per-project `PinMap`, addressed the same way a `ButtonView`/`ToggleView` addresses its commands/reports. |
| `PeripheralFactory.h`, `PeripheralManager.h/.cpp`, `Factory.h` | Registry/dispatch for peripherals (and, via the shared `riot2::Factory<T>` template, the pattern each project's own `ViewFactory` reuses) keyed by `classFullName`. |
| `Command.h`, `Report.h`, `DeviceConfiguration.h`, `Topics.h` | Wire-format data structures and centralized MQTT topic strings, kept in sync with `RIoT2.Core.Constants`/`RIoT2.Core.Models`. |
| `Feedback.h` | `IFeedback` interface (`tap()/confirm()/error()/alert()/ring()/vibrate(ms)`) each project's own buzzer/haptics implementation conforms to; `vibrate()` defaults to a no-op for boards without a vibration motor. |
| `scripts/generate_manifest.py` | PlatformIO pre-build script: generates `include/Manifest.h`/`src/Manifest.cpp` (embedded build info, used in the MQTT online announcement) and a `manifest.json` build artifact, reading the consuming project's own `MANIFEST_NAME`/`VERSION` files so one script serves every project correctly. |

## Adding a consumer project

1. Add the `lib_extra_dirs`/`extra_scripts` lines above to the new project's `platformio.ini`.
2. Add a one-line `MANIFEST_NAME` file at the new project's root (e.g. `RIoT2 M5Stick Node`) and a
   `VERSION` file (e.g. `0.1.0`) — both read by `generate_manifest.py`.
3. `#include <riot2/...>` whichever shared headers are needed; implement the project-local pieces
   (display/UI, `IFeedback`, any board-specific peripheral pin maps) on top.

## Regression gate

Deterministic host regressions for the sibling firmware projects are available:

```powershell
python tests\test_firmware_p1.py
python tests\test_firmware_p2.py
```

Run from a Visual Studio C++ developer shell on Windows, or provide a native
C++14 compiler using `CXX` (defaults to `cl` or `c++`). Python uses only its standard
library. All four firmware repositories should be checked out as siblings.
The tests compile production function bodies/class declarations against minimal
timer and Wi-Fi fakes; they verify button timer ownership and BLE reconnect policy.
The Wiegand regression also compiles the complete USI driver against fake AVR
registers to exercise short reads, aborts, TX overflow status and normal responses.
P2 regressions cover independent Wiegand acquisition, GPIO capabilities, exact MQTT
packet-size boundaries, configuration retry deadlines and both boards' BLE snapshot restore.
The MQTT test uses ArduinoJson headers already present in either board's `.pio\libdeps`
directory; it fails with a clear message if they are absent and never downloads them.
Generated files stay in `tests\.host-build` and are removed after the run.
These checks do not replace board builds or real LVGL/radio integration tests.

### Bounded firmware policies

- **GPIO:** `GpioPinMap::outputMask` describes output support for A1/A2/B1/B2.
  All four are enabled by default; Core2 excludes input-only B2. Unsupported output
  slots are logged and ignored. Core2's generated relay template selects B1/GPIO26.
- **MQTT:** online/offline/report JSON is serialized completely or rejected with a
  diagnostic. The existing 512-byte packet limit is retained (or the smaller runtime
  client buffer); five header bytes, two topic-length bytes and the topic itself are
  reserved. A 54-byte topic therefore permits exactly 451 JSON bytes, not 452.
  Oversized messages are not fragmented or queued, and publish failures are logged.
  Diagnostic reason codes: 1=document allocation overflow, 2=packet too large,
  3=serialization incomplete, 4=transport publish failed. Payloads are not logged.
- **Configuration:** each new MQTT configuration notification replaces any pending URL
  and schedules an immediate main-loop attempt. Failures retry after 1, 2, 4, 8, 16,
  then 30 seconds, staying at 30 seconds until success or a replacement notification.
  No attempts run while Wi-Fi is disconnected; retry timing uses rollover-safe elapsed
  milliseconds measured after a failed request completes. Success clears the request.
  HTTP requests remain synchronous; the retry scheduler does not eliminate their latency.
- **BLE:** rebuilt consumers receive a main-loop snapshot of devices seen within the
  last 60 seconds. Snapshot restoration updates the UI silently rather than replaying
  `deviceFound` reports and potentially retriggering automations. Genuine new discoveries,
  advertisements and subsequent device-loss reports keep their existing behavior.

Because two node projects depend on this library, **any change here must be build-verified in
both consumers** before it's considered done:

```powershell
cd ..\RIoT2.Ard.M5Dial.Node  && pio run
cd ..\RIoT2.Ard.M5Core2.Node && pio run
```
