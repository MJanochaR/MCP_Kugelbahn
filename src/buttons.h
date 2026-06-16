#pragma once

class Button {
public:
    explicit Button(int pin);

    void begin();
    bool pressed();
    bool isPressed() const;

private:
    int _pin;
    bool _stableState;
    bool _lastReading;
    unsigned long _lastChange;
};