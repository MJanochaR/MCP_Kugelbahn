#pragma once

#include <Arduino.h>

// Generic debounced digital input using INPUT_PULLUP semantics by default.
class Input {
public:
    // pin: Arduino pin number
    // activeLow: if true, a LOW reading means "active" (typical for INPUT_PULLUP)
    explicit Input(int pin, unsigned long debounceMs = 50, bool activeLow = true);

    // configure pin mode and initial state
    void begin();

    // call from loop() to update internal state (optional, pressed() will also poll)
    void update();

    // returns true when a new active edge (inactive->active) was detected since last call
    bool triggered();

    // immediate level check (true when currently active)
    bool isActive() const;

private:
    int _pin;
    unsigned long _debounceMs;
    bool _activeLow;

    bool _stableState;
    bool _lastReading;
    unsigned long _lastChange;
    bool _pendingTrigger;
};

// Light barrier sensor: same behaviour as Input but named for clarity.
class LightBarrier : public Input {
public:
    explicit LightBarrier(int pin, unsigned long debounceMs = 50, bool activeLow = true)
        : Input(pin, debounceMs, activeLow) {}
};
