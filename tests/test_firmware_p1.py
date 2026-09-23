"""Native regressions for firmware P1 fixes; no board SDK or hardware required.

Run from the shared repo: python tests/test_firmware_p1.py
Use a Visual Studio developer shell on Windows, or CXX=c++ elsewhere.
Production function bodies and class declarations are compiled against small
deterministic fakes. This exercises logic, not Arduino/LVGL/AVR integration.
"""

import os
from pathlib import Path
import shutil
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(__file__).resolve().parent / ".host-build"
CORE = ROOT / "RIoT2.Ard.M5Core2.Node"
DIAL = ROOT / "RIoT2.Ard.M5Dial.Node"
SHARED = ROOT / "RIoT2.Ard.Shared" / "RIoT2Shared"
WIEGAND = ROOT / "RIoT2.Ard.WiegandI2C"


def body(path, signature):
    text = path.read_text(encoding="utf-8")
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def declaration(path):
    return "\n".join(
        line for line in path.read_text(encoding="utf-8").splitlines()
        if not line.startswith(("#include", "#pragma"))
    )


def compile_and_run(name, source):
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise RuntimeError("Host C++ compiler missing: use a developer shell or set CXX")
    BUILD.mkdir(exist_ok=True)
    source_path = BUILD / (name + ".cpp")
    executable = BUILD / (name + (".exe" if os.name == "nt" else ""))
    source_path.write_text(source, encoding="utf-8")
    if Path(compiler).name.lower() in ("cl", "cl.exe"):
        command = [compiler, "/nologo", "/EHsc", "/std:c++14",
                   str(source_path), "/Fe:" + str(executable),
                   "/Fo:" + str(BUILD / (name + ".obj"))]
    else:
        command = [compiler, "-std=c++14", "-Wall", "-Wextra",
                   str(source_path), "-o", str(executable)]
    for invocation in (command, [str(executable)]):
        result = subprocess.run(invocation, cwd=BUILD, timeout=60,
                                capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(
                f"{name} failed ({result.returncode})\n{result.stdout}\n{result.stderr}"
            )


class FirmwareP1Tests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def test_button_timer_lifetime(self):
        source = (Path(__file__).parent / "button_timer.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_CLASS",
                                declaration(CORE / "include" / "ButtonView.h"))
        cpp = CORE / "src" / "ButtonView.cpp"
        signatures = [
            "ButtonView::~ButtonView()", "void ButtonView::cancelFlashTimers()",
            "void ButtonView::begin(", "void ButtonView::matrixEventCb(",
            "void ButtonView::flashTimerCb(",
        ]
        source = source.replace("// PRODUCTION_METHODS",
                                "\n".join(body(cpp, s) for s in signatures))
        compile_and_run("button_timer", source)

    def test_dial_ble_reconnect_policy(self):
        source = (Path(__file__).parent / "dial_reconnect.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_WIFI_CLASS",
                                declaration(SHARED / "include" / "riot2" / "WifiConnection.h"))
        cpp = SHARED / "src" / "WifiConnection.cpp"
        signatures = ["void WifiConnection::begin(", "void WifiConnection::startConnection()",
                      "void WifiConnection::loop()"]
        source = source.replace("// PRODUCTION_WIFI_METHODS",
                                "\n".join(body(cpp, s) for s in signatures))
        activation = body(DIAL / "src" / "main.cpp",
                          "if (viewManager.hasBleConsumer() && !bleActive)")
        source = source.replace("// PRODUCTION_BLE_ACTIVATION", activation)
        compile_and_run("dial_reconnect", source)

    def test_wiegand_read_boundaries_and_overflow(self):
        source = (Path(__file__).parent / "wiegand_reads.cpp").read_text(encoding="utf-8")
        source = source.replace("// PRODUCTION_DRIVER_HEADER",
                                declaration(WIEGAND / "usiTwiSlave.h"))
        source = source.replace("// PRODUCTION_DRIVER",
                                declaration(WIEGAND / "usiTwiSlave.c"))
        source = source.replace("// PRODUCTION_WRAPPER_CLASS",
                                declaration(WIEGAND / "TinyWireS.h"))
        source = source.replace("// PRODUCTION_SEND",
                                body(WIEGAND / "TinyWireS.cpp", "bool USI_TWI_S::send("))
        source = source.replace("// PRODUCTION_REQUEST",
                                body(WIEGAND / "WiegandI2C.ino", "void requestEvent()"))
        compile_and_run("wiegand_reads", source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
