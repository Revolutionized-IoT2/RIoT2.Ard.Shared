"""Firmware command results and desired state (I19; designs 7.1 phase 3 and 7.2 phase 5).

NodeRuntime's real handlers, CorrelationRing and NodeStatus run on the host against
tests/runtime_harness.h: the firmware rows of design 7.2's compatibility matrix, the command result
rules (executed, duplicate, rejected, unknownCommand, failed) and the packet budget of the status.

Run from the shared repo after a board build (for ArduinoJson): python tests/test_desired_state.py
"""

import shutil
import unittest

from test_firmware_p1 import BUILD, SHARED, body, compile_and_run
from test_golden_messages import TESTS, runtime_include_dirs, runtime_source


class DesiredStateTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def test_results_and_desired_state(self):
        source = runtime_source((TESTS / "desired_state.cpp").read_text(encoding="utf-8"))
        output = compile_and_run("desired_state", source, runtime_include_dirs())
        self.assertEqual("ok", output.strip())

    def test_runtime_wiring(self):
        runtime = (SHARED / "src" / "NodeRuntime.cpp").read_text(encoding="utf-8")
        begin = body(SHARED / "src" / "NodeRuntime.cpp", "void NodeRuntime::begin(")
        self.assertIn("_mqtt.setCapabilities({Capabilities::CommandResult, Capabilities::DesiredState});", begin)
        self.assertIn("_mqtt.onConnected([this]() { publishStatus(); });", begin)
        # An OTA command installed before the restart is answered `duplicate` after it.
        self.assertLess(begin.index("takeInstalledOtaCommand()"), begin.index("_mqtt.begin(_config);"))
        loop = body(SHARED / "src" / "NodeRuntime.cpp", "void NodeRuntime::loop()")
        self.assertIn("configurationFetched(fetched);", loop)
        self.assertEqual(runtime.count("OtaRollback::markHealthy();"), 1)

        mqtt = SHARED / "src" / "MqttConnection.cpp"
        connect = body(mqtt, "void MqttConnection::attemptConnect()")
        for topic in ("Topics::orchestratorOnline(), 1)", "Topics::configuration(_config.id).c_str(), 1)",
                      "Topics::command(_config.id).c_str(), 1)"):
            self.assertIn("_client.subscribe(" + topic, connect)
        # The status goes out before the presence, as on the .NET node.
        self.assertLess(connect.index("_connectedCallback()"), connect.index("publishOnline();"))

        # Divergence D5 is fixed: the configuration path is lowercase, as in the contract.
        client = (SHARED / "src" / "OrchestratorClient.cpp").read_text(encoding="utf-8")
        self.assertIn('"api/nodes/"', client)
        self.assertNotIn("api/Nodes", client)
        self.assertIn('http.addHeader("If-None-Match"', client)
        self.assertIn("HTTP_CODE_NOT_MODIFIED", client)

        ota = body(SHARED / "src" / "OtaUpdater.cpp", "bool OtaUpdater::performUpdate(")
        self.assertIn("httpUpdate.rebootOnUpdate(false);", ota)
        self.assertLess(ota.index("onInstalled();"), ota.index("ESP.restart();"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
