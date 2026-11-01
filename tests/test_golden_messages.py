"""Golden-file contract test for the firmware (I05, plan M7 step 3); no board SDK or hardware.

NodeRuntime's MQTT handlers (both boards, since I18) and OrchestratorClient::parseConfiguration
read the golden files in RIoT2.Core/contracts/golden, which must be cloned next to this
repository. The expected output is computed here with Python's json module.

Run from the shared repo: python tests/test_golden_messages.py
"""

import json
from pathlib import Path
import shutil
import unittest

from test_firmware_p1 import BUILD, CORE, DIAL, ROOT, SHARED, body, compile_and_run, declaration

GOLDEN = ROOT / "RIoT2.Core" / "contracts" / "golden"
TESTS = Path(__file__).resolve().parent
TYPES = ("boolean", "number", "text", "entity", "textarray")
# The NodeRuntime members that tests/runtime_harness.h compiles.
HANDLERS = ("void NodeRuntime::handleCommand(", "void NodeRuntime::completeCommand(",
            "void NodeRuntime::publishCommandResult(", "void NodeRuntime::publishStatus()",
            "void NodeRuntime::handleConfigurationMessage(", "void NodeRuntime::configurationFetched(",
            "void NodeRuntime::handleConfiguration(")


def runtime_source(test_source):
    """Pastes tests/runtime_harness.h, with the production code, over // RUNTIME_HARNESS."""
    harness = (TESTS / "runtime_harness.h").read_text(encoding="utf-8")
    harness = harness.replace("// PRODUCTION_SOURCES", "\n".join(
        f'#include "{(SHARED / "src" / name).as_posix()}"' for name in ("CommandResults.cpp", "NodeStatus.cpp")))
    runtime = SHARED / "src" / "NodeRuntime.cpp"
    harness = harness.replace("// PRODUCTION_HANDLERS", "\n".join(body(runtime, s) for s in HANDLERS))
    return test_source.replace("// RUNTIME_HARNESS", harness)


def runtime_include_dirs():
    return [TESTS / "host", SHARED / "include", arduino_json_headers()]


def golden(relative):
    path = GOLDEN / relative
    if not path.exists():
        raise AssertionError(f"{path} is missing. Clone RIoT2.Core next to RIoT2.Ard.Shared.")
    return path.read_text(encoding="utf-8")


def literal(text):
    return 'R"golden(' + text + ')golden"'


def arduino_json_headers():
    for board in (CORE, DIAL):
        headers = list((board / ".pio" / "libdeps").glob("*/ArduinoJson/src/ArduinoJson.h"))
        if headers:
            return headers[0].parent
    raise AssertionError("ArduinoJson headers missing: run pio run for a board first")


def parameters(values):
    return ",".join(f"{k}={v}" for k, v in (values or {}).items())


def expected_output():
    lines = []
    for t in TYPES:
        command = json.loads(golden(f"messages/command-{t}.json"))
        value = json.dumps(command["value"], separators=(",", ":"), ensure_ascii=False)
        lines.append(f"command|{command['id']}|{value}")
    lines.append("configuration|")
    lines.append("configuration-hash|" + json.loads(golden("messages/configuration-command-hash.json"))["hash"])

    node = json.loads(golden("http/node-configuration.json"))
    lines.append(f"node|{node['id']}|{node['name']}|{node.get('revision', -1)}|{node.get('hash') or ''}")
    for device in node["deviceConfigurations"]:
        lines.append(f"device|{device['id']}|{device['name']}|{device['classFullName']}|"
                     + parameters(device.get("deviceParameters")))
        for command in device.get("commandTemplates") or []:
            # `type` is RIoT2.Core's ValueType as a number (divergence C2, fixed in I13).
            model = 1 if command.get("model") is True else 0
            lines.append(f"  command|{command['id']}|{command['name']}|{command['address']}"
                         f"|model={model}|type={command.get('type', 0)}")
        for report in device.get("reportTemplates") or []:
            lines.append(f"  report|{report['id']}|{report['name']}|{report['address']}|"
                         f"type={report.get('type', 0)}|" + parameters(report.get("parameters")))
    hashed = json.loads(golden("http/node-configuration-hash.json"))
    lines.append(f"node-hash|{hashed['revision']}|{hashed['hash']}")
    return lines


def same_shape(test, produced, golden_message, context):
    """Every field the firmware writes is in the golden message, with the same JSON type."""
    for key, value in produced.items():
        test.assertIn(key, golden_message, f"{context}: {key} is not in the contract")
        test.assertIs(type(value), type(golden_message[key]), f"{context}: {key}")


class GoldenMessageTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def test_runtime_reads_the_golden_messages(self):
        source = runtime_source((TESTS / "golden_messages.cpp").read_text(encoding="utf-8"))
        source = source.replace("// PRODUCTION_PARSE_CONFIGURATION",
                                body(SHARED / "src" / "OrchestratorClient.cpp",
                                     "bool OrchestratorClient::parseConfiguration("))
        source = source.replace("// PRODUCTION_SERIALIZE_CONFIGURATIONS",
                                body(SHARED / "src" / "DeviceConfigurationJson.cpp",
                                     "String riot2::serializeDeviceConfigurations("))
        data = ["const char* const kGoldenCommands[] = {"]
        data += [literal(golden(f"messages/command-{t}.json")) + "," for t in TYPES]
        data += ["};"]
        for name, path in [("kGoldenConfigurationCommand", "messages/configuration-command.json"),
                           ("kGoldenConfigurationCommandHash", "messages/configuration-command-hash.json"),
                           ("kGoldenCorrelatedCommand", "messages/correlated-command.json"),
                           ("kGoldenNodeConfiguration", "http/node-configuration.json"),
                           ("kGoldenNodeConfigurationHash", "http/node-configuration-hash.json")]:
            data.append(f"const char* const {name} = {literal(golden(path))};")
        source = source.replace("// GOLDEN_DATA", "\n".join(data))
        output = compile_and_run("golden_messages", source, runtime_include_dirs()).splitlines()

        written = {line.split("|", 1)[0]: json.loads(line.split("|", 1)[1]) for line in output
                   if line.split("|", 1)[0] in ("status", "status-failed", "result", "result-unknown")}
        self.assertEqual(expected_output(), [line for line in output if line.split("|", 1)[0] not in written])

        # Design 7.2: the status as the orchestrator reads it.
        applied = json.loads(golden("messages/node-status-applied.json"))
        failed = json.loads(golden("messages/node-status-failed.json"))
        hashed = json.loads(golden("http/node-configuration-hash.json"))
        same_shape(self, written["status"], applied, "status")
        self.assertEqual(written["status"]["state"], "applied")
        self.assertEqual(written["status"]["appliedRevision"], hashed["revision"])
        self.assertEqual(written["status"]["appliedHash"], hashed["hash"])
        self.assertEqual(written["status"]["failedDevices"], applied["failedDevices"])
        same_shape(self, written["status-failed"], {**applied, **failed}, "status-failed")
        self.assertEqual(written["status-failed"]["error"], failed["error"])
        self.assertEqual(written["status-failed"]["appliedHash"], hashed["hash"])

        # Design 7.1: the command results.
        executed = json.loads(golden("messages/result-executed.json"))
        unknown = json.loads(golden("messages/result-failed.json"))
        same_shape(self, written["result"], executed, "result")
        for key in ("correlationId", "commandId", "status"):
            self.assertEqual(written["result"][key], executed[key], key)
        same_shape(self, written["result-unknown"], unknown, "result-unknown")
        self.assertEqual(written["result-unknown"]["correlationId"], unknown["correlationId"])
        self.assertEqual(written["result-unknown"]["status"], "unknownCommand")


if __name__ == "__main__":
    unittest.main(verbosity=2)
