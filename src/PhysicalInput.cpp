#include "PhysicalInput.h"
#include "Devices.h"

ButtonInput::ButtonInput(uint8_t pin, uint8_t buttonId, IDevice* linkedDevice, ButtonMode mode) 
    : _pin(pin), _buttonId(buttonId), _lastState(false), 
      _lastDebounceTime(0), _linkedDevice(linkedDevice), _mode(mode) {
    
    if (_mode == ButtonMode::ACTIVE_LOW) {
        pinMode(_pin, INPUT_PULLUP);
    } else {
        pinMode(_pin, INPUT);
    }
    
    EventSystem::instance().addListener(this, EventType::DeviceStateChanged);
}

void ButtonInput::update() {
    bool currentReading = digitalRead(_pin);
    
    if (_mode == ButtonMode::ACTIVE_LOW) {
        currentReading = !currentReading;
    }
    
    if (currentReading && !_lastState) {
        if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
            onButtonPressed();
        }
        _lastDebounceTime = millis();
    }
    
    _lastState = currentReading;
}

void ButtonInput::onButtonPressed() {
    // OTTIMIZZAZIONE: Rimozione emit non necessario (già gestito da toggle)
    // EventSystem::instance().emit(EventType::ButtonPressed, nullptr, _buttonId);
    
    if (_linkedDevice) {
        if (_linkedDevice->getType() == DeviceType::LightSimple) {
            static_cast<SimpleLight*>(_linkedDevice)->toggle();
        } 
        else if (_linkedDevice->getType() == DeviceType::LightDimmable) {
            static_cast<DimmableLight*>(_linkedDevice)->toggle();
        }
    }
}

void ButtonInput::handleEvent(EventType type, IDevice* device, int value) {}

InputManager& InputManager::instance() {
    static InputManager inst;
    return inst;
}

void InputManager::registerButton(ButtonInput* button) {
    _buttons.add(button);
}

void InputManager::updateAll() {
    for (uint8_t i = 0; i < _buttons.size(); i++) {
        _buttons[i]->update();
    }
}
