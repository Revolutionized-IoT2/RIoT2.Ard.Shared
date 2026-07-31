#include <riot2/Uuid.h>

#include <esp_system.h>

String riot2::newId() {
    uint8_t b[16];
    for (int i = 0; i < 16; i++) {
        b[i] = static_cast<uint8_t>(esp_random() & 0xFF);
    }
    b[6] = (b[6] & 0x0F) | 0x40;  // version 4
    b[8] = (b[8] & 0x3F) | 0x80;  // variant 1

    char buf[37];
    snprintf(buf, sizeof(buf), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0], b[1],
              b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return String(buf);
}
