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
    bool pressed_event = false;

    if (reading != _lastReading) {
        _lastChange = millis();
    }

    if ((millis() - _lastChange) >= BUTTON_DEBOUNCE_MS) {
        if (reading != _stableState) {
            bool oldState = _stableState;
            _stableState = reading;

            if (oldState == HIGH && _stableState == LOW) {
                pressed_event = true;
            }
        }
    }
    
    _lastReading = reading;
    return pressed_event;
}

bool Button::isPressed() const {
    return digitalRead(_pin) == LOW;
}