#pragma once

#include <ArduinoJson.h>
#include <cstring>

namespace riot2 {
enum class JsonPublishResult {
    Published,
    DocumentOverflow,
    PacketTooLarge,
    SerializationFailed,
    PublishFailed
};

// PubSubClient reserves five header bytes and two bytes for topic length.
template <size_t Capacity, typename Client>
JsonPublishResult publishJson(Client& client, const char* topic, const JsonDocument& doc, bool retained) {
    if (doc.overflowed()) return JsonPublishResult::DocumentOverflow;
    const size_t clientCapacity = client.getBufferSize();
    const size_t packetCapacity = clientCapacity < Capacity ? clientCapacity : Capacity;
    const size_t topicLength = std::strlen(topic);
    const size_t payloadLength = measureJson(doc);
    if (packetCapacity < 7 || topicLength > packetCapacity - 7 ||
        payloadLength > packetCapacity - 7 - topicLength) {
        return JsonPublishResult::PacketTooLarge;
    }
    char payload[Capacity];
    if (serializeJson(doc, payload, sizeof(payload)) != payloadLength) {
        return JsonPublishResult::SerializationFailed;
    }
    return client.publish(topic, reinterpret_cast<const uint8_t*>(payload), payloadLength, retained)
               ? JsonPublishResult::Published
               : JsonPublishResult::PublishFailed;
}
}  // namespace riot2
