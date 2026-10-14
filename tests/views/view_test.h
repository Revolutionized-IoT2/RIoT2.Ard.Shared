// Helpers for the view-model tests; tests/test_view_models.py compiles each *_test.cpp in this
// folder together with every model in RIoT2Shared/src/views.
#pragma once

#include <cstdio>
#include <cstdlib>

#include <ArduinoJson.h>
#include <riot2/Command.h>

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);              \
            std::exit(1);                                                                   \
        }                                                                                   \
    } while (0)

// A command whose value is parsed from JSON. Not copyable: Command::value points into `document`.
struct TestCommand {
    JsonDocument document;
    Command command;
    TestCommand(const char* id, const char* json) {
        CHECK(!deserializeJson(document, json));
        command.id = id;
        command.value = document.as<JsonVariantConst>();
    }
    TestCommand(const TestCommand&) = delete;
    TestCommand& operator=(const TestCommand&) = delete;
};

inline DeviceConfiguration withParameters(DeviceConfiguration config, ParameterList parameters) {
    config.deviceParameters = parameters;
    return config;
}
