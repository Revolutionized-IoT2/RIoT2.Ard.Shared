"""Native regressions for rendering-independent Dial timer execution."""

from pathlib import Path
import shutil
import unittest

from test_firmware_p1 import BUILD, CORE, DIAL, SHARED, body, compile_and_run, declaration
from test_view_models import arduino_json_headers

TESTS = Path(__file__).resolve().parent


class FirmwareArchitectureTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def test_dial_timer_background_lifecycle(self):
        source = (TESTS / "dial_timer.cpp").read_text(encoding="utf-8")
        # The real TimerModel runs inside the spliced Dial TimerView and ViewManager.
        source = source.replace("// PRODUCTION_MODELS", "\n".join(
            f'#include "{(SHARED / "src" / "views" / name).as_posix()}"'
            for name in ("ViewSupport.cpp", "TimerModel.cpp")
        ))
        for marker, path in [
            ("IVIEW", DIAL / "include" / "IView.h"),
            ("TIMER", DIAL / "include" / "TimerView.h"),
            ("MANAGER", DIAL / "include" / "ViewManager.h"),
        ]:
            source = source.replace("// PRODUCTION_" + marker, declaration(path))
        timer = DIAL / "src" / "TimerView.cpp"
        signatures = [
            "void TimerView::begin(", "void TimerView::onTouch(", "void TimerView::onEncoderChange(",
            "void TimerView::onCommand(", "void TimerView::loop()", "void TimerView::render(",
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
        render = body(timer, "void TimerView::render(")
        for effect in ("Buzzer::", "finishIfElapsed", "publishReport", "ringDue"):
            self.assertNotIn(effect, render)
        # The countdown lives in the shared model only (plan M5): no board keeps its own clock.
        for root in (CORE, DIAL):
            self.assertNotIn("millis()", (root / "src" / "TimerView.cpp").read_text(encoding="utf-8"), root.name)
        compile_and_run("dial_timer", source,
                        [TESTS / "host", SHARED / "include", arduino_json_headers()])


if __name__ == "__main__":
    unittest.main(verbosity=2)