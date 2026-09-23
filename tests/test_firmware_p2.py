"""Run firmware P2 native regressions using the existing P1 host runner."""

from pathlib import Path
import json
import shutil
import unittest

from test_firmware_p1 import BUILD, CORE, DIAL, SHARED, WIEGAND, body, compile_and_run, declaration


class FirmwareP2Tests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def test_gpio_output_capabilities(self):
        source = (Path(__file__).parent / "gpio_capabilities.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_CLASS",
                                declaration(SHARED / "include" / "riot2" / "GpioPeripheral.h"))
        source = source.replace("// PRODUCTION_METHODS",
                                declaration(SHARED / "src" / "GpioPeripheral.cpp"))
        maps = []
        for root, name in [(CORE, "kM5Core2GroveMap"), (DIAL, "kM5DialGroveMap")]:
            text = (root / "src" / "main.cpp").read_text(encoding="utf-8")
            start = text.index("constexpr GpioPinMap " + name)
            maps.append(text[start:text.index(";", start) + 1])
        source = source.replace("// PRODUCTION_MAPS", "\n".join(maps))
        template = (CORE / "src" / "main.cpp").read_text(encoding="utf-8")
        gpio_start = template.index('"RIoT2.Ard.M5Core2.Node.GpioPeripheral",')
        gpio_end = template.index("return config;", gpio_start)
        self.assertIn('cmd.address = "B1";', template[gpio_start:gpio_end])
        sample = json.loads((CORE / "test" / "sample-node-configuration.json").read_text(encoding="utf-8"))
        for device in sample["deviceConfigurations"]:
            if device["classFullName"].endswith(".GpioPeripheral"):
                for command in device.get("commandTemplates", []):
                    self.assertIn(command["address"], ("A1", "A2", "B1"))
        compile_and_run("gpio_capabilities", source)

    def test_wiegand_independent_frames(self):
        source = (Path(__file__).parent / "wiegand_frames.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_SKETCH", declaration(WIEGAND / "WiegandI2C.ino"))
        compile_and_run("wiegand_frames", source)

    def test_configuration_retry(self):
        source = (Path(__file__).parent / "configuration_retry.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_CLASS",
                                declaration(SHARED / "include" / "riot2" / "ConfigurationRetry.h"))
        for root in (CORE, DIAL):
            main = (root / "src" / "main.cpp").read_text(encoding="utf-8")
            self.assertIn("configurationRetry.request(apiBaseUrl);", main)
            self.assertIn("configurationRetry.loop(wifi.isConnected()", main)
        compile_and_run("configuration_retry", source)

    def test_mqtt_complete_bounded_json(self):
        # Reuse an already-installed ArduinoJson; never download dependencies.
        headers = list((CORE / ".pio" / "libdeps").glob("*/ArduinoJson/src/ArduinoJson.h"))
        if not headers:
            headers = list((DIAL / ".pio" / "libdeps").glob("*/ArduinoJson/src/ArduinoJson.h"))
        if not headers:
            self.fail("Existing ArduinoJson headers missing; MQTT native regression requires them")
        source = (Path(__file__).parent / "mqtt_json.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_HELPER",
                                declaration(SHARED / "include" / "riot2" / "MqttJson.h"))
        cpp = SHARED / "src" / "MqttConnection.cpp"
        signatures = ["void publishDocument(", "void MqttConnection::publishOnline()",
                      "void MqttConnection::publishReport(", "void MqttConnection::publishOfflineAndDisconnect()"]
        source = source.replace("// PRODUCTION_METHODS", "\n".join(body(cpp, s) for s in signatures))
        compile_and_run("mqtt_json", source, [headers[0].parent])

    def test_ble_rebuild_snapshot(self):
        source = (Path(__file__).parent / "ble_snapshot.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_TYPES",
                                declaration(SHARED / "include" / "riot2" / "BleTypes.h"))
        source = source.replace("// PRODUCTION_SCANNER_CLASS",
                                declaration(SHARED / "include" / "riot2" / "BleScanner.h"))
        scanner = SHARED / "src" / "BleScanner.cpp"
        methods = ["BleScanner& BleScanner::instance()", "BleDeviceInfo* BleScanner::findDevice(",
                   "std::vector<BleDeviceInfo> BleScanner::snapshot()", "void BleScanner::loop()"]
        source = source.replace("// PRODUCTION_SCANNER_METHODS",
                                "\n".join(body(scanner, m) for m in methods))
        for root, name in [(CORE, "Core"), (DIAL, "Dial")]:
            cls = declaration(root / "include" / "BLEView.h").replace("BLEView", name + "BLEView")
            source = source.replace("// PRODUCTION_" + name.upper() + "_CLASS", cls)
            methods = ["void BLEView::onBleSnapshot(", "void BLEView::onBleDeviceDiscovered(",
                       "void BLEView::onBleDeviceLost(", "bool BLEView::isAddressAllowed("]
            if root == DIAL:
                methods.append("void BLEView::clampScrollOffset()")
            cpp = root / "src" / "BLEView.cpp"
            code = "\n".join(body(cpp, m) for m in methods).replace("BLEView::", name + "BLEView::")
            source = source.replace("// PRODUCTION_" + name.upper() + "_METHODS", code)
            restore = body(root / "src" / "ViewManager.cpp", "if (view->consumesBleEvents())")
            source = source.replace("// RESTORE_" + name.upper(), restore)
        compile_and_run("ble_snapshot", source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
