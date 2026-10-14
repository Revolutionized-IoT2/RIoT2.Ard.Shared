#include "riot2/views/ViewSupport.h"

#include <riot2/Uuid.h>

#include <cstdio>
#include <vector>

namespace riot2 {
namespace views {

std::vector<Slot> pairByAddress(const DeviceConfiguration& config, size_t maxSlots) {
    std::vector<Slot> slots;
    for (const auto& report : config.reportTemplates) {
        if (slots.size() >= maxSlots) {
            break;
        }
        Slot slot;
        slot.report = report;
        for (const auto& command : config.commandTemplates) {
            if (report.address.length() > 0 && command.address == report.address) {
                slot.command = command;
                slot.hasCommand = true;
                break;
            }
        }
        slots.push_back(slot);
    }
    return slots;
}

int findSlotByCommand(const std::vector<Slot>& slots, const String& commandId) {
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].hasCommand && slots[i].command.id == commandId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool commandBool(const Command& command) {
    return command.value.is<bool>() ? command.value.as<bool>() : command.value.as<int>() != 0;
}

int commandInt(const Command& command) {
    return command.value.as<int>();
}

String commandText(const Command& command) {
    if (command.value.is<const char*>()) {
        return String(command.value.as<const char*>());
    }
    std::vector<char> buffer(measureJson(command.value) + 1);
    serializeJson(command.value, buffer.data(), buffer.size());
    return String(buffer.data());
}

String boolLiteral(bool value) {
    return value ? String("true") : String("false");
}

String intLiteral(long value) {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%ld", value);
    return String(buffer);
}

String stringLiteral(const String& value) {
    String json = "\"";
    for (size_t i = 0; i < value.length(); ++i) {
        char c = value[i];
        switch (c) {
            case '"': json += "\\\""; break;
            case '\\': json += "\\\\"; break;
            case '\n': json += "\\n"; break;
            case '\r': json += "\\r"; break;
            case '\t': json += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char escaped[8];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(c));
                    json += escaped;
                } else {
                    json += c;
                }
        }
    }
    json += "\"";
    return json;
}

long intParameter(const DeviceConfiguration& config, const char* key, long defaultValue) {
    for (const auto& kv : config.deviceParameters) {
        if (kv.first == key) {
            return kv.second.toInt();
        }
    }
    return defaultValue;
}

DeviceConfiguration makeTemplate(const String& classFullName, const String& name) {
    DeviceConfiguration config;
    config.id = riot2::newId();
    config.name = name;
    config.classFullName = classFullName;
    return config;
}

CommandTemplate makeCommand(const String& name, const String& address, int type) {
    CommandTemplate command;
    command.id = riot2::newId();
    command.type = type;
    command.name = name;
    command.address = address;
    return command;
}

ReportTemplate makeReport(const String& name, const String& address, int type) {
    ReportTemplate report;
    report.id = riot2::newId();
    report.type = type;
    report.name = name;
    report.address = address;
    return report;
}

}  // namespace views
}  // namespace riot2
