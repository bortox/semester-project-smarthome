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
    
    // FIX: Registra per eventi del device linkato (per aggiornare stato locale)
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
    // FIX: Emetti evento invece di chiamare direttamente toggle()
    // Questo ripristina l'Observer Pattern e mantiene il disaccoppiamento
    if (_linkedDevice) {
        EventSystem::instance().emit(EventType::ButtonPressed, _linkedDevice, _buttonId);
    }
}

void ButtonInput::handleEvent(EventType type, IDevice* device, int value) {
    // FIX: Reagisci agli eventi del device linkato per aggiornare UI se necessario
    if (type == EventType::DeviceStateChanged && device == _linkedDevice) {
        // Opzionale: feedback LED bottone, log, etc.
    }
}

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
