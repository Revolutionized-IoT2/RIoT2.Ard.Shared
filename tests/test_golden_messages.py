"""Golden-file contract test for the firmware (I05, plan M7 step 3); no board SDK or hardware.

The board MQTT handlers (M5Core2 and M5Dial main.cpp) and OrchestratorClient::parseConfiguration
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
TYPES = ("boolean", "number", "text", "entity", "textarray")


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
    lines.append("configuration|" + json.loads(golden("messages/configuration-command.json"))["apiBaseUrl"])

    node = json.loads(golden("http/node-configuration.json"))
    lines.append(f"node|{node['id']}|{node['name']}")
    for device in node["deviceConfigurations"]:
        lines.append(f"device|{device['id']}|{device['name']}|{device['classFullName']}|"
                     + parameters(device.get("deviceParameters")))
        for command in device.get("commandTemplates") or []:
            # Divergence C2 (.github docs/contracts/configuration.md): firmware reads `type` as a
            # string and `valueType` as a number, but the orchestrator sends a numeric `type` and
            # no `valueType`. Update this when C2 is fixed.
            model = 1 if command.get("model") is True else 0
            lines.append(f"  command|{command['id']}|{command['name']}|{command['address']}"
                         f"|model={model}|type=|valueType=0")
        for report in device.get("reportTemplates") or []:
            lines.append(f"  report|{report['id']}|{report['name']}|{report['address']}|"
                         + parameters(report.get("parameters")))
    return lines


class GoldenMessageTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def run_board(self, board):
        source = (Path(__file__).parent / "golden_messages.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_CONFIGURATION_TYPES",
                                declaration(SHARED / "include" / "riot2" / "DeviceConfiguration.h"))
        source = source.replace("// PRODUCTION_COMMAND_TYPE",
                                declaration(SHARED / "include" / "riot2" / "Command.h"))
        source = source.replace("// PRODUCTION_PARSE_CONFIGURATION",
                                body(SHARED / "src" / "OrchestratorClient.cpp",
                                     "bool OrchestratorClient::parseConfiguration("))
        main = board / "src" / "main.cpp"
        source = source.replace("// PRODUCTION_HANDLERS", "\n".join(
            body(main, s) for s in ("void handleCommand(", "void handleConfigurationMessage(")))
        data = ["const char* const kGoldenCommands[] = {"]
        data += [literal(golden(f"messages/command-{t}.json")) + "," for t in TYPES]
        data += ["};",
                 "const char* const kGoldenConfigurationCommand = "
                 + literal(golden("messages/configuration-command.json")) + ";",
                 "const char* const kGoldenNodeConfiguration = "
                 + literal(golden("http/node-configuration.json")) + ";"]
        source = source.replace("// GOLDEN_DATA", "\n".join(data))
        output = compile_and_run("golden_messages_" + board.name.split(".")[2].lower(), source,
                                 [arduino_json_headers()])
        self.assertEqual(expected_output(), output.splitlines())

    def test_m5core2_reads_the_golden_messages(self):
        self.run_board(CORE)

    def test_m5dial_reads_the_golden_messages(self):
        self.run_board(DIAL)


if __name__ == "__main__":
    unittest.main(verbosity=2)
