"""View-model tests (plan M5, I17): the shared models in RIoT2Shared/src/views on the host.

Each tests/views/*_test.cpp is compiled together with every model, the Arduino.h shim in
tests/host and the ArduinoJson headers that PlatformIO installed for a board (run pio run first).

Run from the shared repo: python tests/test_view_models.py
"""

from pathlib import Path
import shutil
import unittest

from test_firmware_p1 import BUILD, CORE, DIAL, SHARED, compile_and_run

TESTS = Path(__file__).resolve().parent
MODELS = sorted((SHARED / "src" / "views").glob("*.cpp"))


def arduino_json_headers():
    for board in (CORE, DIAL):
        headers = list((board / ".pio" / "libdeps").glob("*/ArduinoJson/src/ArduinoJson.h"))
        if headers:
            return headers[0].parent
    raise AssertionError("ArduinoJson headers missing: run pio run for a board first")


def translation_unit(test):
    lines = [
        "#include <Arduino.h>",
        "unsigned long hostMillis = 0;",
        "#include <riot2/Uuid.h>",
        "namespace riot2 { String newId() { static int n = 0; return String(\"id-\") + String(++n); } }",
    ]
    lines += [f'#include "{model.as_posix()}"' for model in MODELS]
    lines.append(f'#include "{test.as_posix()}"')
    return "\n".join(lines) + "\n"


class ViewModelTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        if BUILD.exists():
            shutil.rmtree(BUILD)

    def test_every_model_has_a_test(self):
        tested = {path.name[:-len("_test.cpp")] for path in (TESTS / "views").glob("*_test.cpp")}
        models = {path.stem for path in MODELS} - {"ViewSupport"}
        self.assertEqual(set(), models - tested, "Add tests/views/<Model>_test.cpp")

    def test_view_models(self):
        include = [TESTS / "host", SHARED / "include", TESTS / "views", arduino_json_headers()]
        for test in sorted((TESTS / "views").glob("*_test.cpp")):
            with self.subTest(test.stem):
                compile_and_run("view_" + test.stem, translation_unit(test), include)


if __name__ == "__main__":
    unittest.main(verbosity=2)
