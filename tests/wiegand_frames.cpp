#include <cassert>
#include <cstdint>
#include <vector>
#define ISR(vector) void vector()
#define _BV(bit) (1 << (bit))
enum { PB1 = 1, PB3 = 3, PB4 = 4, PCINT3 = 3, PCINT4 = 4, PCIE = 5,
       TOIE1 = 2, CS13 = 3, CS12 = 2, CS11 = 1 };
uint8_t DDRB, PORTB, PINB, PCMSK, GIMSK, TIMSK, TCCR1, TCNT1, SREG;
void cli() { SREG &= 0x7F; }
void sei() { SREG |= 0x80; }
struct Wire {
    std::vector<uint8_t> sent;
    void begin(uint8_t) {}
    void onRequest(void (*)()) {}
    bool send(uint8_t value) { sent.push_back(value); return true; }
} TinyWireS;
void requestEvent();
void codeReceived();
// PRODUCTION_SKETCH
void bit(bool one) {
    PINB = one ? _BV(WGD_D0) : _BV(WGD_D1);
    PCINT0_vect();
    PINB = _BV(WGD_D0) | _BV(WGD_D1);
    PCINT0_vect(); // Rising edge must not contribute a bit.
}
uint8_t popcount12(uint32_t value) {
    uint8_t count = 0;
    value &= 0xFFF;
    while (value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}
uint32_t encodeWiegand26(uint32_t code) {
    code &= 0xFFFFFF;
    bool evenParity = (popcount12(code >> 12) % 2) != 0;
    bool oddParity = (popcount12(code) % 2) == 0;
    return (evenParity ? (1u << 25) : 0) | (code << 1) | (oddParity ? 1u : 0);
}
void frame(uint32_t code) {
    uint32_t encoded = encodeWiegand26(code);
    for (int i = 25; i >= 0; --i) bit((encoded >> i) & 1);
    TIM1_OVF_vect();
}
void rawFrame(uint32_t encoded) {
    for (int i = 25; i >= 0; --i) bit((encoded >> i) & 1);
    TIM1_OVF_vect();
}
uint32_t read() {
    TinyWireS.sent.clear();
    requestEvent();
    assert(TinyWireS.sent.size() == 3);
    return (TinyWireS.sent[0] << 16) | (TinyWireS.sent[1] << 8) | TinyWireS.sent[2];
}
int main() {
    setup();
    rawFrame(encodeWiegand26(0x123456) ^ (1u << 25));
    assert(!hasNewData && completedCode == 0 && counter == 0 && buffer == 0 && read() == 0);
    frame(0x123456);
    assert(hasNewData && completedCode == 0x123456 && counter == 0 && buffer == 0);
    frame(0xABCDEF);
    assert(wiegandDroppedFrames() == 1 && completedCode == 0x123456);
    assert(PORTB & _BV(WGD_IRQ));
    bit(true); // Invalid partial frame must not erase the pending result.
    TIM1_OVF_vect();
    assert(hasNewData && completedCode == 0x123456 && (PORTB & _BV(WGD_IRQ)));
    assert(read() == 0x123456 && !hasNewData && !(PORTB & _BV(WGD_IRQ)));
    assert(read() == 0);
    frame(0x654321);
    uint32_t next = 0x0FEDCB;
    uint32_t encoded = encodeWiegand26(next);
    for (int i = 25; i >= 13; --i) bit((encoded >> i) & 1);
    uint64_t partial = buffer;
    uint8_t partialCount = counter;
    assert(read() == 0x654321 && buffer == partial && counter == partialCount);
    for (int i = 12; i >= 0; --i) bit((encoded >> i) & 1);
    TIM1_OVF_vect();
    assert(read() == next);
    encoded = encodeWiegand26(0xABCDEFu);
    for (int i = 25; i >= 13; --i) bit((encoded >> i) & 1);
    partial = buffer;
    partialCount = counter;
    assert(read() == 0 && buffer == partial && counter == partialCount);
    for (int i = 12; i >= 0; --i) bit((encoded >> i) & 1);
    TIM1_OVF_vect();
    assert(read() == 0xABCDEF);
    frame(0); // Zero remains a valid ready code, distinguished by IRQ.
    assert(hasNewData && (PORTB & _BV(WGD_IRQ)));
    for (unsigned i = 0; i < 282; ++i) bit(false);
    TIM1_OVF_vect(); // Oversized frame must not wrap the bit count back to 26.
    assert(hasNewData && wiegandDroppedFrames() == 1);
    droppedFrameCount = UINT32_MAX;
    frame(1);
    assert(wiegandDroppedFrames() == UINT32_MAX);
    SREG = 0x80;
    assert(wiegandDroppedFrames() == UINT32_MAX && SREG == 0x80);
    SREG = 0;
    assert(wiegandDroppedFrames() == UINT32_MAX && SREG == 0);
}
