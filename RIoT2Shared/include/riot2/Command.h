#pragma once

#include <ArduinoJson.h>

// Mirrors RIoT2.Core.Models.Command: an inbound riot2/{id}/command message
// addressed to one of the active configuration's commandTemplates[].id.
// `value` is loosely-typed JSON (bool, number, string, or object) - inspect
// with JsonVariantConst::is<T>() / as<T>().
struct Command {
    String id;
    JsonVariantConst value;
};
