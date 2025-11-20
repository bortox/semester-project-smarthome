#include "ModulinoKnob.h"

ModulinoKnob::ModulinoKnob(uint8_t address, TwoWire* wire)
    : _wire(wire), _address(address), _lastPosition(0), _lastDebounceTime(0),
      _currentPressed(false), _lastPressed(false), _pressStartTime(0), _lastReleaseTime(0),
      _clickDetected(false), _doubleClickDetected(false), 
      _longPressDetected(false), _longPressHandled(false) {}

bool ModulinoKnob::begin() {
    _wire->begin();
    _wire->setClock(100000);
    
    if (!scan()) {
        return false;
    }
    
    // Leggi posizione iniziale
    _lastPosition = getPosition();
    _lastDebounceTime = millis();
    
    return true;
}

int16_t ModulinoKnob::getPosition() {
    uint8_t buf[3];
    if (!read(buf, 3)) {
        return 0;
    }
    
    _currentPressed = (buf[2] != 0);
    int16_t position = buf[0] | (buf[1] << 8);
    return position;
}

void ModulinoKnob::setPosition(int16_t value) {
    uint8_t buf[4];
    memcpy(buf, &value, 2);
    buf[2] = 0;  // Padding
    buf[3] = 0;
    write(buf, 4);
}

int8_t ModulinoKnob::getDirection() {
    unsigned long now = millis();
    if (now - _lastDebounceTime < DEBOUNCE_DELAY) {
        return 0;
    }
    
    int16_t current = getPosition();
    int8_t direction = 0;
    
    if (current > _lastPosition) {
        direction = 1;   // Clockwise
    } else if (current < _lastPosition) {
        direction = -1;  // Counter-clockwise
    }
    
    if (direction != 0) {
        _lastDebounceTime = now;
        _lastPosition = current;
    }
    
    return direction;
}

bool ModulinoKnob::isPressed() {
    getPosition();  // Aggiorna _currentPressed
    return _currentPressed;
}

void ModulinoKnob::update() {
    unsigned long now = millis();
    bool pressed = isPressed();
    
    // Reset flags consumabili
    _clickDetected = false;
    _doubleClickDetected = false;
    
    // Rileva RISING EDGE (press)
    if (pressed && !_lastPressed) {
        _pressStartTime = now;
        _longPressHandled = false;
    }
    
    // Rileva FALLING EDGE (release)
    if (!pressed && _lastPressed) {
        unsigned long pressDuration = now - _pressStartTime;
        
        // Se era long press, ignora click
        if (pressDuration >= LONG_PRESS_DURATION) {
            _longPressDetected = false;  // Reset dopo rilascio
        } 
        // Altrimenti è un click
        else if (pressDuration > 50) {  // Debounce minimo
            // Controlla double click
            if (now - _lastReleaseTime < DOUBLE_CLICK_WINDOW) {
                _doubleClickDetected = true;
            } else {
                _clickDetected = true;
            }
            _lastReleaseTime = now;
        }
    }
    
    // Rileva long press (durante pressione)
    if (pressed && !_longPressHandled) {
        if (now - _pressStartTime >= LONG_PRESS_DURATION) {
            _longPressDetected = true;
            _longPressHandled = true;  // Triggera solo una volta
        }
    }
    
    _lastPressed = pressed;
}

bool ModulinoKnob::wasClicked() {
    return _clickDetected;
}

bool ModulinoKnob::wasDoubleClicked() {
    return _doubleClickDetected;
}

bool ModulinoKnob::isLongPress() {
    return _longPressDetected;
}

// --- I2C Private Methods ---

bool ModulinoKnob::read(uint8_t* buf, int howmany) {
    _wire->requestFrom(_address, (uint8_t)(howmany + 1));
    
    unsigned long start = millis();
    while (_wire->available() == 0 && (millis() - start < 100)) {
        delay(1);
    }
    
    if (_wire->available() < howmany) {
        return false;
    }
    
    uint8_t pinstrap = _wire->read();  // Primo byte (indirizzo pinstrap)
    for (int i = 0; i < howmany; i++) {
        buf[i] = _wire->read();
    }
    
    // Flush buffer
    while (_wire->available()) {
        _wire->read();
    }
    
    return true;
}

bool ModulinoKnob::write(uint8_t* buf, int howmany) {
    _wire->beginTransmission(_address);
    for (int i = 0; i < howmany; i++) {
        _wire->write(buf[i]);
    }
    return (_wire->endTransmission() == 0);
}

bool ModulinoKnob::scan() {
    _wire->beginTransmission(_address);
    return (_wire->endTransmission() == 0);
}
