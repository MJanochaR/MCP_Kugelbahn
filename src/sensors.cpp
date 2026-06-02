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

AnalogLightBarrier::AnalogLightBarrier(int pin, int threshold, unsigned long debounceMs)
    : _pin(pin), _threshold(threshold), _debounceMs(debounceMs),
      _stableActive(false), _lastActive(false), _lastChange(0),
      _pendingTrigger(false), _lastValue(0) {
}

void AnalogLightBarrier::begin() {
    pinMode(_pin, INPUT);
    _lastValue = analogRead(_pin);
    _stableActive = _lastValue >= _threshold;
    _lastActive = _stableActive;
    _lastChange = millis();
}

void AnalogLightBarrier::update() {
    _lastValue = analogRead(_pin);
    bool active = _lastValue >= _threshold;

    if (active != _lastActive) {
        _lastChange = millis();
        _lastActive = active;
    }

    if (millis() - _lastChange >= _debounceMs && active != _stableActive) {
        bool wasActive = _stableActive;
        _stableActive = active;

        if (!wasActive && _stableActive) {
            _pendingTrigger = true;
        }
    }
}

bool AnalogLightBarrier::triggered() {
    update();

    if (_pendingTrigger) {
        _pendingTrigger = false;
        return true;
    }
    return false;
}

bool AnalogLightBarrier::isActive() const {
    return _stableActive;
}

int AnalogLightBarrier::value() const {
    return _lastValue;
}
