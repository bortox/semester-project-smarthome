#pragma once

#include <Arduino.h>
#include "lights.h"
#include "sensors.h"

// Classe base astratta per i controller
class LightController {
public:
    virtual ~LightController() {}
    virtual void update() = 0;
};

// Controller per pulsante toggle
class ButtonToggleController : public LightController {
private:
    Light* _light;
    uint8_t _pin;
    int _lastButtonState = HIGH;
    unsigned long _lastDebounceTime = 0;
    int _buttonState;
    const long DEBOUNCE_DELAY = 50;

public:
    ButtonToggleController(Light* light, uint8_t buttonPin) 
        : _light(light), _pin(buttonPin) {
        pinMode(_pin, INPUT_PULLUP);
    }

    void update() override {
        int reading = digitalRead(_pin);
        if (reading != _lastButtonState) {
            _lastDebounceTime = millis();
        }
        if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
            if (reading != _buttonState) {
                _buttonState = reading;
                if (_buttonState == LOW) {
                    _light->toggle();
                }
            }
        }
        _lastButtonState = reading;
    }
};

// Controller potenziometro
template<class DimmableType>
class PotentiometerDimmerController : public LightController {
private:
    DimmableType* _light;
    uint8_t _pin;
    int _lastMappedValue = -1;
    const int THRESHOLD = 2;

public:
    PotentiometerDimmerController(DimmableType* light, uint8_t analogPin)
        : _light(light), _pin(analogPin) {}

    void update() override {
        int rawValue = analogRead(_pin);
        int mappedValue = map(rawValue, 0, 1023, 0, 100);

        if (abs(mappedValue - _lastMappedValue) >= THRESHOLD) {
            _light->setBrightness(mappedValue);
            _lastMappedValue = mappedValue;
        }
    }
};

// Controller luce esterna
class OutsideController : public LightController {
public:
    enum Mode { MODE_OFF, MODE_ON, MODE_AUTO_LIGHT, MODE_AUTO_MOTION };

private:
    SimpleLight& _light;
    LightSensor& _lightSensor;
    MovementSensor& _movementSensor;
    Mode _currentMode = MODE_AUTO_LIGHT;
    
    unsigned long _motion_trigger_time = 0;
    const unsigned long MOTION_LIGHT_DURATION = 30000;

public:
    int light_threshold = 50;

    OutsideController(SimpleLight& light, LightSensor& ls, MovementSensor& ms)
        : _light(light), _lightSensor(ls), _movementSensor(ms) {}

    void setMode(Mode mode) {
        _currentMode = mode;
        update();
    }

    Mode getMode() const { return _currentMode; }
    
    void update() override {
        switch (_currentMode) {
            case MODE_ON:
                _light.setStatus(true);
                break;
            case MODE_OFF:
                _light.setStatus(false);
                break;
            case MODE_AUTO_LIGHT:
                _light.setStatus(_lightSensor.getValue() < light_threshold);
                break;
            case MODE_AUTO_MOTION:
                if (_lightSensor.getValue() < light_threshold) {
                    if (_movementSensor.getValue()) {
                        _light.setStatus(true);
                        _motion_trigger_time = millis();
                    } else if (millis() - _motion_trigger_time > MOTION_LIGHT_DURATION) {
                        _light.setStatus(false);
                    }
                } else {
                    _light.setStatus(false);
                }
                break;
        }
    }
};