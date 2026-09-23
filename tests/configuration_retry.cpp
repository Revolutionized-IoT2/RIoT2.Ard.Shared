#include <cassert>
#include <cstdint>
#include <string>
using String = std::string;
uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
// PRODUCTION_CLASS
int main() {
    ConfigurationRetry retry;
    unsigned calls = 0;
    String received;
    bool success = false;
    auto fetch = [&](const String& url) { ++calls; received = url; return success; };
    retry.loop(true, fetch);
    assert(calls == 0);
    retry.request("first");
    retry.loop(false, fetch);
    assert(calls == 0);
    retry.loop(true, fetch);
    assert(calls == 1 && received == "first");
    for (uint32_t delay : {1000u, 2000u, 4000u, 8000u, 16000u, 30000u, 30000u}) {
        unsigned before = calls;
        nowMs += delay - 1;
        retry.loop(true, fetch);
        assert(calls == before);
        ++nowMs;
        retry.loop(true, fetch);
        assert(calls == before + 1);
    }
    retry.request("replacement");
    retry.loop(true, fetch); // Superseding notification retries immediately.
    assert(received == "replacement");
    unsigned before = calls;
    nowMs += 999;
    retry.loop(true, fetch);
    assert(calls == before);
    ++nowMs;
    success = true;
    retry.loop(true, fetch);
    assert(calls == before + 1);
    nowMs += 60000;
    retry.loop(true, fetch);
    assert(calls == before + 1); // Stops after success.
    success = false;
    nowMs = UINT32_MAX - 499;
    retry.request("wrap");
    retry.loop(true, fetch);
    before = calls;
    nowMs += 999;
    retry.loop(true, fetch);
    assert(calls == before);
    ++nowMs;
    retry.loop(false, fetch);
    assert(calls == before);
    retry.loop(true, fetch);
    assert(calls == before + 1 && received == "wrap");
}
