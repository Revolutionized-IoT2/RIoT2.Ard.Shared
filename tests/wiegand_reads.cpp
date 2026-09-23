#include <cassert>
#include <cstdint>
#include <vector>

// Register/interrupt substitutes; the complete production USI driver runs below.
#define __AVR_ATtiny85__ 1
#define ISR(vector) void vector()
enum {
    PB0 = 0, PB2 = 2, PINB0 = 0, PINB2 = 2,
    USISIF = 7, USIOIF = 6, USIPF = 5, USIDC = 4, USICNT0 = 0,
    USISIE = 7, USIOIE = 6, USIWM1 = 5, USIWM0 = 4,
    USICS1 = 3, USICS0 = 2, USICLK = 1, USITC = 0
};
uint8_t DDRB, PORTB, PINB, USICR, USISR, USIDR;
// PRODUCTION_DRIVER_HEADER
// PRODUCTION_DRIVER
// PRODUCTION_WRAPPER_CLASS
USI_TWI_S::USI_TWI_S() {}
// PRODUCTION_SEND
USI_TWI_S TinyWireS;

volatile uint64_t buffer;
volatile uint8_t counter;
volatile bool hasNewData;
volatile uint32_t completedCode;
#define WGD_OUT_REG PORTB
#define WGD_IRQ 1
#define _BV(bit) (1 << (bit))
// PRODUCTION_REQUEST

uint8_t sequence = 0;
unsigned callbackCalls = 0, accepted = 0, rejected = 0;
void freshResponse() {
    ++callbackCalls;
    ++sequence;
    assert(txCount == 0);
    assert(TinyWireS.send(sequence));
    assert(TinyWireS.send(sequence + 1));
    assert(TinyWireS.send(sequence + 2));
}
void overflowResponse() {
    ++callbackCalls;
    for (unsigned i = 0; i < TWI_TX_BUFFER_SIZE + 4; ++i) {
        bool queued = TinyWireS.send(static_cast<uint8_t>(i));
        assert(queued == (i < TWI_TX_BUFFER_SIZE));
        queued ? ++accepted : ++rejected;
    }
}
void startRead() {
    PINB = 0; // Start has completed: SCL and SDA low.
    USI_START_vect();
    assert(overflowState == USI_SLAVE_CHECK_ADDRESS);
    USIDR = (0x26 << 1) | 1;
    USI_OVF_vect();
    assert(overflowState == USI_SLAVE_SEND_DATA);
}
std::vector<uint8_t> readBytes(unsigned count) {
    std::vector<uint8_t> result;
    if (!count) return result; // Master aborts directly after the address ACK.
    USI_OVF_vect(); // First data byte.
    for (unsigned i = 0; i < count; ++i) {
        assert(overflowState == USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA);
        result.push_back(USIDR);
        USI_OVF_vect(); // Sample master's reply.
        USIDR = (i + 1 == count) ? 1 : 0; // Last byte NACK; earlier bytes ACK.
        USI_OVF_vect();
    }
    return result;
}
int main() {
    usiTwiSlaveInit(0x26);
    usi_onRequestPtr = freshResponse;
    // Repeated short reads previously filled TX and hung on the eighth one-byte read.
    for (unsigned count : {0u, 1u, 2u, 3u}) {
        for (unsigned repetition = 0; repetition < 40; ++repetition) {
            startRead();
            assert(txCount == 3);
            auto bytes = readBytes(count);
            assert(bytes.size() == count && txCount == 3 - count);
            for (unsigned i = 0; i < count; ++i) assert(bytes[i] == sequence + i);
        }
    }
    assert(callbackCalls == 160);

    // Abort while waiting for the ACK of a data byte, then issue repeated START.
    startRead();
    USI_OVF_vect();
    USI_OVF_vect();
    assert(overflowState == USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA);
    startRead();
    auto bytes = readBytes(3);
    assert(bytes[0] == sequence && bytes[2] == sequence + 2 && txCount == 0);

    usi_onRequestPtr = overflowResponse;
    startRead(); // Must return rather than block; overflow is visible through bool send().
    assert(accepted == TWI_TX_BUFFER_SIZE && rejected == 4);
    bytes = readBytes(TWI_TX_BUFFER_SIZE);
    for (unsigned i = 0; i < bytes.size(); ++i) assert(bytes[i] == i);
    assert(txCount == 0);
    usi_onRequestPtr = freshResponse;
    startRead();
    bytes = readBytes(3);
    assert(bytes[0] == sequence && txCount == 0); // Recovery after overflow.

    // Exercise the unchanged sketch callback: MSB first, then zeros on the next read.
    usi_onRequestPtr = requestEvent;
    completedCode = 0x123456;
    buffer = 0x1; // An independently arriving frame must survive reading the pending code.
    counter = 1;
    hasNewData = true;
    PORTB |= _BV(WGD_IRQ);
    startRead();
    bytes = readBytes(3);
    assert((bytes == std::vector<uint8_t>{0x12, 0x34, 0x56}));
    assert(!hasNewData && completedCode == 0 && buffer == 1 && counter == 1 && !(PORTB & _BV(WGD_IRQ)));
    startRead();
    assert((readBytes(3) == std::vector<uint8_t>{0, 0, 0}));
}
