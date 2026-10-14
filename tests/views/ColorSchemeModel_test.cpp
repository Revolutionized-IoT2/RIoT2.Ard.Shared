#include "view_test.h"

#include <cmath>

using namespace riot2::views;

namespace {
bool closeTo(float actual, float expected) {
    return std::fabs(actual - expected) < 0.001f;
}
}  // namespace

int main() {
    auto config = ColorSchemeModel::buildTemplate("RIoT2.Ard.M5Dial.Node.ColorSchemeView");
    CHECK(config.name == "Color Scheme View" && config.classFullName == "RIoT2.Ard.M5Dial.Node.ColorSchemeView");
    CHECK(config.deviceParameters.empty() && config.commandTemplates.size() == 1 && config.reportTemplates.size() == 1);
    CHECK(config.commandTemplates[0].name == "Color" && config.commandTemplates[0].address == "lamp-1");
    CHECK(config.commandTemplates[0].type == riot2::ValueType::Text);
    CHECK(config.reportTemplates[0].name == "Color" && config.reportTemplates[0].address == "lamp-1");
    CHECK(config.reportTemplates[0].type == riot2::ValueType::Text);
    auto core = ColorSchemeModel::buildTemplate("RIoT2.Ard.M5Core2.Node.ColorSchemeView");
    CHECK(core.classFullName == "RIoT2.Ard.M5Core2.Node.ColorSchemeView" && core.name == "Color Scheme View");

    // Hex parsing and formatting.
    ColorSchemeModel::Rgb rgb;
    CHECK(ColorSchemeModel::parseHex("#A0b1C2", rgb) && rgb.red == 0xA0 && rgb.green == 0xB1 && rgb.blue == 0xC2);
    CHECK(ColorSchemeModel::parseHex("0A0B0C", rgb) && rgb.red == 0x0A && rgb.blue == 0x0C);
    CHECK(ColorSchemeModel::toHex(rgb) == "#0A0B0C");
    CHECK(!ColorSchemeModel::parseHex("#12345", rgb) && rgb.red == 255 && rgb.green == 0 && rgb.blue == 0);
    CHECK(ColorSchemeModel::parseHex("#GG10ZZ", rgb) && rgb.red == 0 && rgb.green == 0x10 && rgb.blue == 0);

    // LVGL's integer HSV math, including its rounding.
    CHECK(ColorSchemeModel::toHex(ColorSchemeModel::hsvToRgb(0, 100, 100)) == "#FF0000");
    CHECK(ColorSchemeModel::toHex(ColorSchemeModel::hsvToRgb(120, 100, 100)) == "#03FF00");
    CHECK(ColorSchemeModel::toHex(ColorSchemeModel::hsvToRgb(200, 0, 50)) == "#7F7F7F");
    int h = -1, s = -1, v = -1;
    ColorSchemeModel::parseHex("#000080", rgb);
    ColorSchemeModel::rgbToHsv(rgb, h, s, v);
    CHECK(h == 240 && s == 100 && v == 50);
    ColorSchemeModel::parseHex("#FF00FF", rgb);
    ColorSchemeModel::rgbToHsv(rgb, h, s, v);
    CHECK(h == 300 && s == 100 && v == 100);
    ColorSchemeModel::parseHex("#808080", rgb);
    ColorSchemeModel::rgbToHsv(rgb, h, s, v);
    CHECK(h == 0 && s == 0 && v == 50);

    // HSV picker (Core2).
    ColorSchemeModel hsv(ColorSchemeModel::Picker::Hsv);
    hsv.begin(core);
    CHECK(hsv.hue() == 0 && hsv.saturation() == 100 && hsv.value() == 100 && hsv.hex() == "#FF0000");
    Report report;
    CHECK(hsv.canReport() && hsv.reportColor(report));
    CHECK(report.id == core.reportTemplates[0].id && report.value == "\"#FF0000\"");
    hsv.setHue(400);
    hsv.setSaturationValue(-5, 150);
    CHECK(hsv.hue() == 359 && hsv.saturation() == 0 && hsv.value() == 100 && hsv.hex() == "#FFFFFF");
    hsv.setHue(-1);
    CHECK(hsv.hue() == 0);

    TestCommand green(core.commandTemplates[0].id.c_str(), "\"#00FF00\""),
        navy(core.commandTemplates[0].id.c_str(), "\"000080\""), bad(core.commandTemplates[0].id.c_str(), "\"#12\""),
        number(core.commandTemplates[0].id.c_str(), "123456"), other("other", "\"#00FF00\"");
    CHECK(hsv.onCommand(green.command) && hsv.hue() == 120 && hsv.saturation() == 100 && hsv.value() == 100);
    CHECK(hsv.onCommand(navy.command) && hsv.hue() == 240 && hsv.value() == 50);
    CHECK(!hsv.onCommand(other.command) && hsv.hue() == 240);
    CHECK(hsv.onCommand(bad.command) && hsv.hue() == 0 && hsv.saturation() == 100 && hsv.value() == 100);
    CHECK(hsv.onCommand(number.command) && hsv.hex() != "#FF0000");
    // The HSV picker never picks.
    CHECK(!hsv.isPicking());

    DeviceConfiguration bare = core;
    bare.reportTemplates.clear();
    bare.commandTemplates.clear();
    hsv.begin(bare);
    CHECK(!hsv.canReport() && !hsv.reportColor(report) && !hsv.onCommand(green.command));

    // Shade picker (Dial).
    ColorSchemeModel dial(ColorSchemeModel::Picker::Shade);
    dial.begin(config);
    CHECK(closeTo(dial.mainHue(), 0.0f) && closeTo(dial.shade(), 0.5f) && dial.hex() == "#FF0000");
    CHECK(ColorSchemeModel::toHex(ColorSchemeModel::shadeToRgb(120.0f, 0.0f)) == "#000000");
    CHECK(ColorSchemeModel::toHex(ColorSchemeModel::shadeToRgb(120.0f, 1.0f)) == "#FFFFFF");
    CHECK(ColorSchemeModel::toHex(ColorSchemeModel::shadeToRgb(-240.0f, 0.5f)) == "#00FF00");

    TestCommand dialGreen(config.commandTemplates[0].id.c_str(), "\"#00FF00\""),
        darkGreen(config.commandTemplates[0].id.c_str(), "\"#008000\""),
        paleGreen(config.commandTemplates[0].id.c_str(), "\"#80FF80\"");
    CHECK(dial.onCommand(dialGreen.command) && closeTo(dial.mainHue(), 120.0f) && closeTo(dial.shade(), 0.5f));
    CHECK(dial.hex() == "#00FF00");
    CHECK(dial.onCommand(darkGreen.command) && closeTo(dial.mainHue(), 120.0f) && closeTo(dial.shade(), 128.0f / 510.0f));
    CHECK(dial.onCommand(paleGreen.command) && closeTo(dial.mainHue(), 120.0f) && closeTo(dial.shade(), 1.0f - 127.0f / 510.0f));
    CHECK(!dial.onCommand(green.command));

    // Picking starts at the nearest main color, steps through the twelve and confirms a shade.
    CHECK(!dial.confirm(0.5f, report));
    CHECK(dial.startPicking() && dial.isPicking() && !dial.startPicking());
    CHECK(closeTo(dial.shownHue(), 120.0f) && closeTo(dial.shownShade(), 0.5f) && closeTo(dial.shade(), 1.0f - 127.0f / 510.0f));
    CHECK(!dial.onCommand(dialGreen.command) && closeTo(dial.mainHue(), 120.0f));
    dial.step(3);
    CHECK(closeTo(dial.shownHue(), 150.0f) && closeTo(dial.mainHue(), 120.0f));
    for (int i = 0; i < 6; ++i) {
        dial.step(-1);
    }
    CHECK(closeTo(dial.shownHue(), 330.0f));
    dial.step(0);
    CHECK(closeTo(dial.shownHue(), 300.0f));
    dial.step(2);
    CHECK(dial.confirm(0.25f, report) && !dial.isPicking());
    CHECK(closeTo(dial.mainHue(), 330.0f) && closeTo(dial.shade(), 0.25f) && closeTo(dial.shownShade(), 0.25f));
    CHECK(dial.hex() == "#7F003F" && report.id == config.reportTemplates[0].id && report.value == "\"#7F003F\"");

    // Picking from between two main colors rounds to the nearest, wrapping at 360.
    TestCommand orange(config.commandTemplates[0].id.c_str(), "\"#FF4000\""),
        rose(config.commandTemplates[0].id.c_str(), "\"#FF0020\"");
    CHECK(dial.onCommand(orange.command) && dial.startPicking() && closeTo(dial.shownHue(), 30.0f));
    CHECK(dial.confirm(ColorSchemeModel::kPureShade, report) && report.value == "\"#FF7F00\"");
    CHECK(dial.onCommand(rose.command) && dial.startPicking() && closeTo(dial.shownHue(), 0.0f));

    // A new configuration stops picking; without a report template picking never starts.
    dial.begin(config);
    CHECK(!dial.isPicking() && closeTo(dial.mainHue(), 0.0f));
    DeviceConfiguration noReport = config;
    noReport.reportTemplates.clear();
    dial.begin(noReport);
    CHECK(!dial.startPicking() && !dial.isPicking() && !dial.confirm(0.5f, report));
    std::printf("ColorSchemeModel ok\n");
}
