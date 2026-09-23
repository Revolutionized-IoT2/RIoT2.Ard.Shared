#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <ArduinoJson.h>
#define MQTT_MAX_PACKET_SIZE 512
using String = std::string;
struct PubSubClient {
    size_t capacity = 512;
    bool connectedValue = true, publishOk = true, retained = false;
    unsigned calls = 0;
    String topic, payload;
    size_t getBufferSize() const { return capacity; }
    bool connected() const { return connectedValue; }
    void disconnect() { connectedValue = false; }
    bool publish(const char* t, const uint8_t* bytes, size_t size, bool retain) {
        ++calls;
        topic = t; payload.assign(reinterpret_cast<const char*>(bytes), size); retained = retain;
        return publishOk;
    }
};
struct FailedAllocator : ArduinoJson::Allocator {
    void* allocate(size_t) override { return nullptr; }
    void deallocate(void*) override {}
    void* reallocate(void*, size_t) override { return nullptr; }
};
// PRODUCTION_HELPER
struct Log {
    unsigned errors = 0;
    template<typename... Args> void printf(const char*, Args...) { ++errors; }
} Serial;
struct IPAddress {
    int value;
    IPAddress(int a, int b, int c, int d) : value(a + b + c + d) {}
    bool operator!=(const IPAddress& other) { return value != other.value; }
    String toString() { return "192.0.2.1"; }
};
struct Wifi { IPAddress localIP() { return {192, 0, 2, 1}; } } WiFi;
namespace Topics {
String online(const String& id) { return "riot2/node/" + id + "/online"; }
String report(const String& id) { return "riot2/node/" + id + "/report"; }
}
struct Report { String id, value; };
enum class MqttState { Connected, Disconnected };
struct MqttConnection {
    struct Config { String id = "00000000-0000-0000-0000-000000000000", name = "Node"; } _config;
    const char* _manifestJson =
        R"({"name":"RIoT2 M5Core2 Node","version":"0.1.0","date":"2026-09-23T00:00:00","installedPackageFilename":"firmware.bin"})";
    PubSubClient _client;
    MqttState _state = MqttState::Connected;
    void publishOnline();
    void publishReport(const Report&);
    void publishOfflineAndDisconnect();
};
// PRODUCTION_METHODS
int main() {
    MqttConnection mqtt;
    mqtt._config.name = String(248, 'N'); // 451-byte JSON: exact budget for 54-byte topic.
    mqtt.publishOnline();
    assert(mqtt._client.calls == 1 && mqtt._client.payload.size() == 451 && mqtt._client.retained);
    JsonDocument parsed;
    assert(!deserializeJson(parsed, mqtt._client.payload));
    assert(parsed["name"].as<String>() == mqtt._config.name);
    mqtt._config.name += 'N';
    mqtt.publishOnline();
    assert(mqtt._client.calls == 1 && Serial.errors == 1); // 452 bytes rejected whole.
    JsonDocument value;
    value["address"] = "AA:BB:CC:DD:EE:FF";
    value["name"] = String(29, '"');
    value["rssi"] = -50;
    value["manufacturerData"] = String(58, 'A');
    String raw;
    serializeJson(value, raw);
    mqtt.publishReport({mqtt._config.id, raw});
    assert(mqtt._client.calls == 2 && mqtt._client.payload.size() > 256 && !mqtt._client.retained);
    assert(!deserializeJson(parsed, mqtt._client.payload));
    assert(parsed["value"]["name"].as<String>() == String(29, '"'));
    mqtt.publishReport({"report", "\"ok\""});
    assert(!deserializeJson(parsed, mqtt._client.payload));
    assert(parsed["value"].as<String>() == "ok");
    unsigned before = mqtt._client.calls;
    mqtt._client.publishOk = false;
    mqtt.publishReport({"report", "true"});
    assert(mqtt._client.calls == before + 1 && Serial.errors == 2);
    mqtt._client.publishOk = true;
    mqtt.publishOfflineAndDisconnect();
    assert(!mqtt._client.connected() && mqtt._state == MqttState::Disconnected);
    assert(mqtt._client.payload == "{\"isOnline\":false}" && mqtt._client.retained);
    PubSubClient client;
    JsonDocument doc;
    doc["value"] = "x";
    client.capacity = 6;
    assert(riot2::publishJson<512>(client, "x", doc, false) == riot2::JsonPublishResult::PacketTooLarge);
    client.capacity = 512;
    assert(riot2::publishJson<512>(client, String(506, 't').c_str(), doc, false) ==
           riot2::JsonPublishResult::PacketTooLarge);
    assert(client.calls == 0);
    FailedAllocator allocator;
    JsonDocument overflowed(&allocator);
    overflowed["value"] = "cannot allocate";
    assert(overflowed.overflowed());
    assert(riot2::publishJson<512>(client, "test", overflowed, false) ==
           riot2::JsonPublishResult::DocumentOverflow);
    assert(client.calls == 0);
}
