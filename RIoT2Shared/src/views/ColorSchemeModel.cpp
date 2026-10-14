#include "riot2/views/ColorSchemeModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace riot2 {
namespace views {

namespace colorscheme {

constexpr float kMainColorStep = 360.0f / ColorSchemeModel::kMainColorCount;

float normalizeHue(float hue) {
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f) {
        hue += 360.0f;
    }
    return hue;
}

// Snaps a hue to the nearest of the fixed main colors, returning its index.
int nearestColorIndex(float hue) {
    int count = ColorSchemeModel::kMainColorCount;
    int index = static_cast<int>(std::round(normalizeHue(hue) / kMainColorStep)) % count;
    if (index < 0) {
        index += count;
    }
    return index;
}

int clampTo(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

uint8_t hexChannel(const char* digits) {
    char pair[3] = {digits[0], digits[1], '\0'};
    return static_cast<uint8_t>(std::strtol(pair, nullptr, 16));
}

}  // namespace colorscheme

void ColorSchemeModel::begin(const DeviceConfiguration& config) {
    _commandId = config.commandTemplates.empty() ? String() : config.commandTemplates.front().id;
    _reportId = config.reportTemplates.empty() ? String() : config.reportTemplates.front().id;
    _hue = 0;
    _saturation = 100;
    _value = 100;
    _mainHue = 0.0f;
    _shade = kPureShade;
    _pendingHue = 0.0f;
    _pendingColorIndex = 0;
    _picking = false;
}

ColorSchemeModel::Rgb ColorSchemeModel::color() const {
    return _picker == Picker::Hsv ? hsvToRgb(_hue, _saturation, _value) : shadeToRgb(_mainHue, _shade);
}

bool ColorSchemeModel::onCommand(const Command& command) {
    if (_picking || _commandId.length() == 0 || command.id != _commandId) {
        return false;
    }
    Rgb rgb;
    parseHex(commandText(command), rgb);
    if (_picker == Picker::Hsv) {
        rgbToHsv(rgb, _hue, _saturation, _value);
    } else {
        rgbToShade(rgb, _mainHue, _shade);
    }
    return true;
}

void ColorSchemeModel::setHue(int hue) {
    _hue = colorscheme::clampTo(hue, 0, 359);
}

void ColorSchemeModel::setSaturationValue(int saturation, int value) {
    _saturation = colorscheme::clampTo(saturation, 0, 100);
    _value = colorscheme::clampTo(value, 0, 100);
}

bool ColorSchemeModel::reportColor(Report& report) const {
    if (!canReport()) {
        return false;
    }
    report = Report{_reportId, stringLiteral(hex())};
    return true;
}

float ColorSchemeModel::shownShade() const {
    if (_picking) {
        return kPureShade;
    }
    return _shade;
}

bool ColorSchemeModel::startPicking() {
    if (_picking || !canReport()) {
        return false;
    }
    _picking = true;
    _pendingColorIndex = colorscheme::nearestColorIndex(_mainHue);
    _pendingHue = _pendingColorIndex * colorscheme::kMainColorStep;
    return true;
}

void ColorSchemeModel::step(int delta) {
    _pendingColorIndex =
        ((_pendingColorIndex + (delta > 0 ? 1 : -1)) % kMainColorCount + kMainColorCount) % kMainColorCount;
    _pendingHue = _pendingColorIndex * colorscheme::kMainColorStep;
}

bool ColorSchemeModel::confirm(float shade, Report& report) {
    if (!_picking) {
        return false;
    }
    _mainHue = _pendingHue;
    _shade = shade;
    _picking = false;
    return reportColor(report);
}

ColorSchemeModel::Rgb ColorSchemeModel::hsvToRgb(int hue, int saturation, int value) {
    // LVGL's lv_color_hsv_to_rgb() (8-bit fixed point), so colors stay as the LVGL picker showed them.
    uint16_t h = static_cast<uint16_t>(static_cast<uint32_t>(static_cast<uint16_t>(hue)) * 255 / 360);
    uint8_t s = static_cast<uint8_t>(static_cast<uint16_t>(static_cast<uint8_t>(saturation)) * 255 / 100);
    uint8_t v = static_cast<uint8_t>(static_cast<uint16_t>(static_cast<uint8_t>(value)) * 255 / 100);

    Rgb rgb;
    if (s == 0) {
        rgb.red = rgb.green = rgb.blue = v;
        return rgb;
    }

    uint8_t region = static_cast<uint8_t>(h / 43);
    uint8_t remainder = static_cast<uint8_t>((h - (region * 43)) * 6);
    uint8_t p = static_cast<uint8_t>((v * (255 - s)) >> 8);
    uint8_t q = static_cast<uint8_t>((v * (255 - ((s * remainder) >> 8))) >> 8);
    uint8_t t = static_cast<uint8_t>((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);

    switch (region) {
        case 0: rgb.red = v; rgb.green = t; rgb.blue = p; break;
        case 1: rgb.red = q; rgb.green = v; rgb.blue = p; break;
        case 2: rgb.red = p; rgb.green = v; rgb.blue = t; break;
        case 3: rgb.red = p; rgb.green = q; rgb.blue = v; break;
        case 4: rgb.red = t; rgb.green = p; rgb.blue = v; break;
        default: rgb.red = v; rgb.green = p; rgb.blue = q; break;
    }
    return rgb;
}

void ColorSchemeModel::rgbToHsv(const Rgb& color, int& hue, int& saturation, int& value) {
    // LVGL's lv_color_rgb_to_hsv() (10-bit fixed point).
    int32_t r = static_cast<int32_t>((static_cast<uint32_t>(color.red) << 10) / 255);
    int32_t g = static_cast<int32_t>((static_cast<uint32_t>(color.green) << 10) / 255);
    int32_t b = static_cast<int32_t>((static_cast<uint32_t>(color.blue) << 10) / 255);

    int32_t rgbMin = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int32_t rgbMax = r > g ? (r > b ? r : b) : (g > b ? g : b);

    value = static_cast<int>((100 * rgbMax) >> 10);

    int32_t delta = rgbMax - rgbMin;
    if (delta < 3) {
        hue = 0;
        saturation = 0;
        return;
    }

    saturation = static_cast<int>(100 * delta / rgbMax);
    if (saturation < 3) {
        hue = 0;
        return;
    }

    int32_t h;
    if (rgbMax == r) {
        h = ((g - b) * 1024) / delta + (g < b ? 6 * 1024 : 0);  // between yellow & magenta
    } else if (rgbMax == g) {
        h = ((b - r) * 1024) / delta + 2 * 1024;  // between cyan & yellow
    } else {
        h = ((r - g) * 1024) / delta + 4 * 1024;  // between magenta & cyan
    }
    // h is never negative here, so dividing matches LVGL's >> 10.
    hue = static_cast<int>(h * 60 / 1024);
}

ColorSchemeModel::Rgb ColorSchemeModel::shadeToRgb(float hue, float shade) {
    // Standard HSV (S=1, V=1) to 8-bit RGB conversion - the "pure" main color.
    float h = colorscheme::normalizeHue(hue) / 60.0f;
    float x = 255.0f * (1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f));

    float rf, gf, bf;
    if (h < 1.0f) {
        rf = 255.0f; gf = x; bf = 0.0f;
    } else if (h < 2.0f) {
        rf = x; gf = 255.0f; bf = 0.0f;
    } else if (h < 3.0f) {
        rf = 0.0f; gf = 255.0f; bf = x;
    } else if (h < 4.0f) {
        rf = 0.0f; gf = x; bf = 255.0f;
    } else if (h < 5.0f) {
        rf = x; gf = 0.0f; bf = 255.0f;
    } else {
        rf = 255.0f; gf = 0.0f; bf = x;
    }
    uint8_t hr = static_cast<uint8_t>(rf);
    uint8_t hg = static_cast<uint8_t>(gf);
    uint8_t hb = static_cast<uint8_t>(bf);

    // Darkening scales channels down (hue-preserving), lightening blends toward white (also
    // hue-preserving, since it's an affine mix).
    Rgb rgb;
    if (shade <= 0.5f) {
        float f = shade / 0.5f;  // 0 (black) -> 1 (pure hue)
        rgb.red = static_cast<uint8_t>(hr * f);
        rgb.green = static_cast<uint8_t>(hg * f);
        rgb.blue = static_cast<uint8_t>(hb * f);
    } else {
        float f = (shade - 0.5f) / 0.5f;  // 0 (pure hue) -> 1 (white)
        rgb.red = static_cast<uint8_t>(hr + (255 - hr) * f);
        rgb.green = static_cast<uint8_t>(hg + (255 - hg) * f);
        rgb.blue = static_cast<uint8_t>(hb + (255 - hb) * f);
    }
    return rgb;
}

void ColorSchemeModel::rgbToShade(const Rgb& color, float& hue, float& shade) {
    // Recovers the (hue, shade) that produced a color, assuming it came from shadeToRgb() above
    // (darkening/lightening both preserve hue exactly, so this round-trips cleanly).
    int r = color.red;
    int g = color.green;
    int b = color.blue;
    int maxV = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int minV = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float delta = static_cast<float>(maxV - minV);

    if (delta < 0.001f) {
        hue = 0.0f;
    } else if (maxV == r) {
        hue = colorscheme::normalizeHue(60.0f * std::fmod((g - b) / delta, 6.0f));
    } else if (maxV == g) {
        hue = colorscheme::normalizeHue(60.0f * ((b - r) / delta + 2.0f));
    } else {
        hue = colorscheme::normalizeHue(60.0f * ((r - g) / delta + 4.0f));
    }

    float value = maxV / 255.0f;
    if (value < 0.999f) {
        shade = value / 2.0f;  // darkened shade
    } else {
        float saturation = maxV == 0 ? 0.0f : delta / static_cast<float>(maxV);
        shade = 1.0f - saturation / 2.0f;  // pure hue or a white-ward tint
    }
}

bool ColorSchemeModel::parseHex(const String& text, Rgb& color) {
    const char* digits = text.c_str();
    if (digits[0] == '#') {
        ++digits;
    }
    if (std::strlen(digits) < 6) {
        color.red = 255;
        color.green = 0;
        color.blue = 0;
        return false;
    }
    color.red = colorscheme::hexChannel(digits);
    color.green = colorscheme::hexChannel(digits + 2);
    color.blue = colorscheme::hexChannel(digits + 4);
    return true;
}

String ColorSchemeModel::toHex(const Rgb& color) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", color.red, color.green, color.blue);
    return String(buffer);
}

DeviceConfiguration ColorSchemeModel::buildTemplate(const String& classFullName) {
    DeviceConfiguration config = makeTemplate(classFullName, "Color Scheme View");
    // Text: a hex color string, e.g. "#FF8800".
    config.commandTemplates.push_back(makeCommand("Color", "lamp-1", riot2::ValueType::Text));
    config.reportTemplates.push_back(makeReport("Color", "lamp-1", riot2::ValueType::Text));
    return config;
}

}  // namespace views
}  // namespace riot2
