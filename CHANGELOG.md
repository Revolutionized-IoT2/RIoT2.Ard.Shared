# Changelog

All notable documentation changes to `RIoT2.Ard.Shared`.

## [Unreleased]

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
