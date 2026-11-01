// Host stand-in for the parts of Arduino.h that the view models and NodeRuntime's handlers use
// (tests/test_view_models.py, tests/runtime_harness.h).
#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

struct String : std::string {
    String() = default;
    String(const char* value) : std::string(value ? value : "") {}
    String(const std::string& value) : std::string(value) {}
    explicit String(int value) : std::string(std::to_string(value)) {}
    explicit String(long value) : std::string(std::to_string(value)) {}
    explicit String(unsigned value) : std::string(std::to_string(value)) {}
    explicit String(unsigned long value) : std::string(std::to_string(value)) {}
    long toInt() const { return std::strtol(c_str(), nullptr, 10); }
    float toFloat() const { return std::strtof(c_str(), nullptr); }
    String substring(size_t from, size_t to) const {
        if (from > size()) from = size();
        if (to > size()) to = size();
        return to > from ? String(substr(from, to - from)) : String();
    }
    // What ArduinoJson's serializeJson() writes through, as into Arduino's String.
    size_t write(uint8_t c) {
        push_back(static_cast<char>(c));
        return 1;
    }
    size_t write(const uint8_t* data, size_t size) {
        append(reinterpret_cast<const char*>(data), size);
        return size;
    }
    bool equalsIgnoreCase(const String& other) const {
        if (size() != other.size()) return false;
        for (size_t i = 0; i < size(); ++i) {
            if (std::tolower(static_cast<unsigned char>((*this)[i])) != std::tolower(static_cast<unsigned char>(other[i]))) return false;
        }
        return true;
    }
};

inline String operator+(const String& left, const char* right) { return String(static_cast<const std::string&>(left) + right); }
inline String operator+(const char* left, const String& right) { return String(left + static_cast<const std::string&>(right)); }
inline String operator+(const String& left, const String& right) {
    return String(static_cast<const std::string&>(left) + static_cast<const std::string&>(right));
}

// Tests advance it to drive time-based models.
extern unsigned long hostMillis;
inline unsigned long millis() { return hostMillis; }
