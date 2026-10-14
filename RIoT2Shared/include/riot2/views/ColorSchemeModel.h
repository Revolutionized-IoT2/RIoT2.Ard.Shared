#pragma once

#include <Arduino.h>

#include <stdint.h>

#include "riot2/views/ViewSupport.h"

namespace riot2 {
namespace views {

// One color, bound to the first command and report template; commands and reports carry it as a
// "#RRGGBB" string. A command sets the color without a report; a color the user confirms
// produces a report.
//
// The color is kept in the form the board's picker edits:
//  - Picker::Hsv: hue 0-359, saturation and value 0-100 (a hue bar and a saturation/value pad),
//    converted with LVGL's integer HSV math.
//  - Picker::Shade: a main hue and a shade (0 = black, 0.5 = the pure hue, 1 = white). Picking
//    starts from the nearest of kMainColorCount main colors, the dial steps through them and a
//    shade confirms the pending one; commands are ignored while picking.
class ColorSchemeModel {
public:
    enum class Picker { Hsv, Shade };

    struct Rgb {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
    };

    static constexpr int kMainColorCount = 12;
    static constexpr float kPureShade = 0.5f;

    explicit ColorSchemeModel(Picker picker) : _picker(picker) {}

    void begin(const DeviceConfiguration& config);

    bool canReport() const { return _reportId.length() > 0; }
    Rgb color() const;
    // "#RRGGBB"
    String hex() const { return toHex(color()); }

    // Sets the color from a "#RRGGBB" command. Returns false for another command id or, with
    // Picker::Shade, while picking.
    bool onCommand(const Command& command);

    // The color as a report. Returns false without a report template.
    bool reportColor(Report& report) const;

    // Picker::Hsv
    int hue() const { return _hue; }
    int saturation() const { return _saturation; }
    int value() const { return _value; }
    // Clamped to 0-359 and 0-100.
    void setHue(int hue);
    void setSaturationValue(int saturation, int value);

    // Picker::Shade
    float mainHue() const { return _mainHue; }
    float shade() const { return _shade; }
    bool isPicking() const { return _picking; }
    // While picking the pending main color as a pure hue, otherwise the confirmed color.
    float shownHue() const { return _picking ? _pendingHue : _mainHue; }
    float shownShade() const;
    // Starts picking at the main color nearest to the confirmed hue. Returns false when already
    // picking or without a report template.
    bool startPicking();
    // Moves the pending main color one step in the direction of `delta`.
    void step(int delta);
    // Confirms the pending main color at `shade` and reports it. Returns false when not picking.
    bool confirm(float shade, Report& report);

    static Rgb hsvToRgb(int hue, int saturation, int value);
    static void rgbToHsv(const Rgb& color, int& hue, int& saturation, int& value);
    static Rgb shadeToRgb(float hue, float shade);
    static void rgbToShade(const Rgb& color, float& hue, float& shade);
    // A "#RRGGBB" (or "RRGGBB") string; false, and red, for fewer than six digits.
    static bool parseHex(const String& text, Rgb& color);
    static String toHex(const Rgb& color);

    static DeviceConfiguration buildTemplate(const String& classFullName);

private:
    Picker _picker;
    String _commandId;
    String _reportId;
    int _hue = 0;
    int _saturation = 100;
    int _value = 100;
    float _mainHue = 0.0f;
    float _shade = kPureShade;
    float _pendingHue = 0.0f;
    int _pendingColorIndex = 0;
    bool _picking = false;
};

}  // namespace views
}  // namespace riot2
