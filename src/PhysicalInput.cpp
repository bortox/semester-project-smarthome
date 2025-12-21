/**
 * @file PhysicalInput.cpp
 * @brief Implementation of physical input handlers
 * @author Andrea Bortolotti
 * @version 2.0
 * @ingroup HAL
 */
#include <Arduino.h>
#include "PhysicalInput.h"
#include "Devices.h"
#include "FlexibleMenu.h"

ButtonInput::ButtonInput(uint8_t pin, uint8_t buttonId, IDevice* linkedDevice, ButtonMode mode)
    : _pin(pin), _buttonId(buttonId), _lastState(false), 
      _lastDebounceTime(0), _linkedDevice(linkedDevice), _mode(mode) {
    if (_mode == ButtonMode::ACTIVE_LOW) {
        pinMode(_pin, INPUT_PULLUP);
    } else {
        pinMode(_pin, INPUT);
    }
    
    if (_linkedDevice) {
        EventSystem::instance().addListener(this, EventType::DeviceStateChanged);
    }
}

void ButtonInput::update() {
    bool reading;
    if (_mode == ButtonMode::ACTIVE_LOW) {
        reading = (digitalRead(_pin) == LOW);
    } else {
        reading = (digitalRead(_pin) == HIGH);
    }
    
    if (reading != _lastState) {
        _lastDebounceTime = millis();
    }
    
    if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading && !_lastState) {
            onButtonPressed();
        }
        _lastState = reading;
    }
}

void ButtonInput::onButtonPressed() {
    if (_linkedDevice) {
        EventSystem::instance().emit(EventType::ButtonPressed, _linkedDevice, _buttonId);
    }
}

void ButtonInput::handleEvent(EventType type, IDevice* device, int value) {
    (void)type;
    (void)device;
    (void)value;
}

PotentiometerInput::PotentiometerInput(uint8_t pin, DimmableLight* linkedLight)
    : _pin(pin), _light(linkedLight), _lastMappedValue(0), _sampleIndex(0) {
    pinMode(_pin, INPUT);
    
    uint16_t initial = analogRead(_pin);
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        _samples[i] = initial;
    }
}

void PotentiometerInput::update() {
    if (!_light) return;
    
    _samples[_sampleIndex] = analogRead(_pin);
    _sampleIndex = (_sampleIndex + 1) % SAMPLE_COUNT;
    
    uint32_t sum = 0;
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        sum += _samples[i];
    }
    uint16_t avg = sum / SAMPLE_COUNT;
    
    uint8_t mappedValue = map(avg, 0, 1023, 0, 100);
    
    if (abs((int)mappedValue - (int)_lastMappedValue) >= POT_CHANGE_THRESHOLD) {
        _lastMappedValue = mappedValue;
        
        if (mappedValue < POT_OFF_THRESHOLD) {
            if (_light->getState()) {
                _light->toggle();
            }
        } else {
            if (!_light->getState()) {
                _light->toggle();
            }
            _light->setBrightness(mappedValue);
        }
    }
}

NavButtonInput::NavButtonInput(uint8_t pin, InputEvent command, ButtonMode mode)
    : _pin(pin), _command(command), _lastState(false), 
      _lastDebounceTime(0), _mode(mode) {
    if (_mode == ButtonMode::ACTIVE_LOW) {
        pinMode(_pin, INPUT_PULLUP);
    } else {
        pinMode(_pin, INPUT);
    }
}

void NavButtonInput::update() {
    bool reading;
    if (_mode == ButtonMode::ACTIVE_LOW) {
        reading = (digitalRead(_pin) == LOW);
    } else {
        reading = (digitalRead(_pin) == HIGH);
    }
    
    if (reading != _lastState) {
        _lastDebounceTime = millis();
    }
    
    if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading && !_lastState) {
            NavigationManager::instance().handleInput(_command);
        }
        _lastState = reading;
    }
}

InputManager& InputManager::instance() {
    static InputManager inst;
    return inst;
}

// cppcheck-suppress unusedFunction
void InputManager::registerButton(ButtonInput* button) {
    _buttons.add(button);
}

// cppcheck-suppress unusedFunction
void InputManager::registerPotentiometer(PotentiometerInput* pot) {
    _potentiometers.add(pot);
}

// cppcheck-suppress unusedFunction
void InputManager::registerNavButton(NavButtonInput* navBtn) {
    _navButtons.add(navBtn);
}

// cppcheck-suppress unusedFunction
void InputManager::updateAll() {
    for (uint8_t i = 0; i < _buttons.size(); i++) {
        _buttons[i]->update();
    }
    for (uint8_t i = 0; i < _potentiometers.size(); i++) {
        _potentiometers[i]->update();
    }
    for (uint8_t i = 0; i < _navButtons.size(); i++) {
        _navButtons[i]->update();
    }
}