/**
 * @file PhysicalInput.cpp
 * @brief Implementation of hardware input handling
 * @ingroup HAL
 */
#include "PhysicalInput.h"
#include "Devices.h"
#include "FlexibleMenu.h"  // For NavigationManager

ButtonInput::ButtonInput(uint8_t pin, uint8_t buttonId, IDevice* linkedDevice, ButtonMode mode) 
    : _pin(pin), _buttonId(buttonId), _lastState(false), 
      _lastDebounceTime(0), _linkedDevice(linkedDevice), _mode(mode) {
    
    if (_mode == ButtonMode::ACTIVE_LOW) {
        pinMode(_pin, INPUT_PULLUP);
    } else {
        pinMode(_pin, INPUT);
    }
    
    // FIX: Register for linked device events (to update local state)
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
    // FIX: Emit event instead of calling toggle() directly
    // This restores the Observer Pattern and maintains decoupling
    if (_linkedDevice) {
        EventSystem::instance().emit(EventType::ButtonPressed, _linkedDevice, _buttonId);
    }
}

void ButtonInput::handleEvent(EventType type, IDevice* device, int value) {
    // FIX: React to linked device events to update UI if necessary
    if (type == EventType::DeviceStateChanged && device == _linkedDevice) {
        // Optional: button LED feedback, log, etc.
    }
}

InputManager& InputManager::instance() {
    static InputManager inst;
    return inst;
}

void InputManager::registerButton(ButtonInput* button) {
    _buttons.add(button);
}

void InputManager::registerPotentiometer(PotentiometerInput* pot) {
    _potentiometers.add(pot);
}

// ====================== NavButtonInput Implementation ======================

NavButtonInput::NavButtonInput(uint8_t pin, InputEvent command, ButtonMode mode)
    : _pin(pin), _command(command), _lastState(false), _lastDebounceTime(0), _mode(mode) {
    
    if (_mode == ButtonMode::ACTIVE_LOW) {
        pinMode(_pin, INPUT_PULLUP);
    } else {
        pinMode(_pin, INPUT);
    }
}

void NavButtonInput::update() {
    bool currentReading = digitalRead(_pin);
    
    if (_mode == ButtonMode::ACTIVE_LOW) {
        currentReading = !currentReading;
    }
    
    if (currentReading && !_lastState) {
        if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
            // Call NavigationManager directly with strongly typed event
            NavigationManager::instance().handleInput(_command);
        }
        _lastDebounceTime = millis();
    }
    
    _lastState = currentReading;
}

// ====================== InputManager Updates ======================

void InputManager::registerNavButton(NavButtonInput* navBtn) {
    _navButtons.add(navBtn);
}


void InputManager::updateAll() {
    // Update standard buttons
    for (size_t i = 0; i < _buttons.size(); i++) {
        _buttons[i]->update();
    }
    
    // Update potentiometers
    for (size_t i = 0; i < _potentiometers.size(); i++) {
        _potentiometers[i]->update();
    }

    // Update navigation buttons (if still used)
    for (size_t i = 0; i < _navButtons.size(); i++) {
        _navButtons[i]->update();
    }

    // NEW: Update knob if registered
    if (_knob != nullptr) {
        _knob->update();
    }
}

// ====================== PotentiometerInput Implementation ======================

PotentiometerInput::PotentiometerInput(uint8_t pin, DimmableLight* light)
    : _pin(pin), _light(light), _lastMappedValue(0), _sampleIndex(0) {
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        _samples[i] = 0;
    }
}

void PotentiometerInput::update() {
    if (!_light) return;

    uint16_t rawValue = analogRead(_pin);

    _samples[_sampleIndex] = rawValue;
    _sampleIndex = (_sampleIndex + 1) % SAMPLE_COUNT;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        sum += _samples[i];
    }
    uint16_t avgValue = sum / SAMPLE_COUNT;

    uint8_t mappedValue = map(avgValue, 0, 1023, 0, 100);

    if (abs((int16_t)mappedValue - (int16_t)_lastMappedValue) >= POT_CHANGE_THRESHOLD) {
        if (mappedValue > POT_OFF_THRESHOLD) {
            _light->setBrightness(mappedValue);
        } else {
            _light->setBrightness(0);
        }
        
        _lastMappedValue = mappedValue;
    }
}


#include "FlexibleMenu.h" // Required for NavigationManager

// --- KnobInput Implementation ---

void KnobInput::update() {
    // Update hardware driver and get event
    KnobEvent e = _hwDriver.update();
    
    // Reference to navigation manager
    NavigationManager& nav = NavigationManager::instance();

    switch (e) {
          case KnobEvent::DOWN: 
            // Clockwise Rotation -> Down in list (Next)
            // Note: In sliders this decrements the value.
            // If you want CW to increment sliders, change DOWN to UP here,
            // but menu navigation will be inverted (CW goes up).
            nav.handleInput(InputEvent::DOWN); 
            break;

        case KnobEvent::UP: 
            // Counter-Clockwise Rotation -> Up in list (Prev)
            nav.handleInput(InputEvent::UP); 
            break;

        case KnobEvent::ENTER: 
            // Single Click -> Enter
            nav.handleInput(InputEvent::ENTER); 
            break;

        case KnobEvent::BACK: 
            // Double Click or Long Press -> Back
            nav.handleInput(InputEvent::BACK); 
            break;

        case KnobEvent::NONE:
        default:
            break;
    }
}