#pragma once

// Common interface for user-facing acknowledgement feedback (buzzer tones,
// haptic vibration, etc.) - lets shared/UI code request semantic feedback
// (tap/confirm/error/alert/ring) without depending on which physical
// feedback mechanism a given board actually has. vibrate() defaults to a
// no-op so boards without a vibration motor (e.g. M5Dial) don't need to
// implement it.
class IFeedback {
public:
    virtual ~IFeedback() = default;

    virtual void tap() {}
    virtual void confirm() {}
    virtual void error() {}
    virtual void alert() {}
    virtual void ring() {}
    virtual void vibrate(unsigned long ms) { (void)ms; }
};
