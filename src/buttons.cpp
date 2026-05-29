#include <Arduino.h>

#include "config.h"
#include "buttons.h"

Button::Button(int pin)
    : _pin(pin),
      _stableState(HIGH),
      _lastReading(HIGH),
      _lastChange(0) {
}

void Button::begin() {
    pinMode(_pin, INPUT_PULLUP);
    _stableState = digitalRead(_pin);
    _lastReading = _stableState;
}

bool Button::pressed() {
    bool reading = digitalRead(_pin);

    if (reading != _lastReading) {
        _lastChange = millis();
        _lastReading = reading;
    }

    if (millis() - _lastChange >= BUTTON_DEBOUNCE_MS && reading != _stableState) {
        bool oldState = _stableState;
        _stableState = reading;

        return oldState == HIGH && _stableState == LOW;
    }

    return false;
}