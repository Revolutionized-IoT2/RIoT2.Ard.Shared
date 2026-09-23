"""Native regressions for rendering-independent Dial timer execution."""

from pathlib import Path
import shutil
import unittest

from test_firmware_p1 import BUILD, DIAL, SHARED, body, compile_and_run, declaration


class FirmwareArchitectureTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def test_dial_timer_background_lifecycle(self):
        source = (Path(__file__).parent / "dial_timer.cpp").read_text(encoding="utf-8")
        for marker, path in [
            ("CONFIG", SHARED / "include" / "riot2" / "DeviceConfiguration.h"),
            ("BLE_TYPES", SHARED / "include" / "riot2" / "BleTypes.h"),
            ("REPORT", SHARED / "include" / "riot2" / "Report.h"),
            ("IVIEW", DIAL / "include" / "IView.h"),
            ("TIMER", DIAL / "include" / "TimerView.h"),
            ("MANAGER", DIAL / "include" / "ViewManager.h"),
        ]:
            source = source.replace("// PRODUCTION_" + marker, declaration(path))
        timer = DIAL / "src" / "TimerView.cpp"
        source = source.replace("// TIMER_CONSTANTS", "\n".join(
            line for line in timer.read_text(encoding="utf-8").splitlines()
            if line.startswith("constexpr ") and "kRing" in line
        ))
        signatures = [
            "void TimerView::begin(", "void TimerView::onTouch(", "void TimerView::onEncoderChange(",
            "void TimerView::onCommand(", "void TimerView::start()", "void TimerView::cancel()",
            "void TimerView::finish()", "int TimerView::remainingSeconds()",
            "void TimerView::loop()", "void TimerView::render(",
        ]
        source = source.replace("// TIMER_METHODS", "\n".join(body(timer, s) for s in signatures))
        manager = DIAL / "src" / "ViewManager.cpp"
        signatures = [
            "void ViewManager::rebuild(", "IView* ViewManager::activeView()",
            "void ViewManager::enterFocused(", "void ViewManager::exitToCarousel()",
            "bool ViewManager::onCommand(", "void ViewManager::loop()",
        ]
        source = source.replace("// MANAGER_METHODS", "\n".join(body(manager, s) for s in signatures))
        main_loop = body(DIAL / "src" / "main.cpp", "void loop()")
        self.assertEqual(main_loop.count("viewManager.loop();"), 1)
        self.assertLess(main_loop.index("viewManager.loop();"), main_loop.index("if (showDiagnostics)"))
        self.assertLess(main_loop.index("viewManager.loop();"), main_loop.index("if (viewManager.hasViews())"))
        self.assertNotIn("Buzzer::", body(timer, "void TimerView::render("))
        self.assertNotIn("finish()", body(timer, "void TimerView::render("))
        compile_and_run("dial_timer", source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
