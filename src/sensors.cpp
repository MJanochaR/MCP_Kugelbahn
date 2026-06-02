#include "sensors.h"

Input::Input(int pin, unsigned long debounceMs, bool activeLow)
    : _pin(pin), _debounceMs(debounceMs), _activeLow(activeLow),
      _stableState(HIGH), _lastReading(HIGH), _lastChange(0), _pendingTrigger(false) {
}

void Input::begin() {
    pinMode(_pin, INPUT_PULLUP);
    _stableState = digitalRead(_pin);
    _lastReading = _stableState;
    _lastChange = millis();
}

void Input::update() {
    bool reading = digitalRead(_pin);

    if (reading != _lastReading) {
        _lastChange = millis();
        _lastReading = reading;
    }

    if (millis() - _lastChange >= _debounceMs && reading != _stableState) {
        bool oldState = _stableState;
        _stableState = reading;

        // detect inactive->active transition according to activeLow
        bool wasActive = _activeLow ? (oldState == LOW) : (oldState == HIGH);
        bool nowActive = _activeLow ? (_stableState == LOW) : (_stableState == HIGH);

        if (!wasActive && nowActive) {
            _pendingTrigger = true;
        }
    }
}

bool Input::triggered() {
    // ensure state is up to date
    update();

    if (_pendingTrigger) {
        _pendingTrigger = false;
        return true;
    }
    return false;
}

bool Input::isActive() const {
    return _activeLow ? (_stableState == LOW) : (_stableState == HIGH);
}
