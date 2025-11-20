#ifndef MODULINO_KNOB_H
#define MODULINO_KNOB_H

#include <Arduino.h>
#include <Wire.h>

class ModulinoKnob {
public:
    ModulinoKnob(uint8_t address = 0x3A, TwoWire* wire = &Wire);
    
    bool begin();
    
    // Lettura posizione encoder
    int16_t getPosition();
    void setPosition(int16_t value);
    
    // Direzione rotazione (-1 = CCW, 0 = no change, +1 = CW)
    int8_t getDirection();
    
    // Stato pulsante
    bool isPressed();
    bool wasClicked();          // Single click rilevato
    bool wasDoubleClicked();    // Double click rilevato
    bool isLongPress();         // Pressione lunga (>800ms)
    
    // Update da chiamare in loop()
    void update();

private:
    TwoWire* _wire;
    uint8_t _address;
    
    // Stato encoder
    int16_t _lastPosition;
    unsigned long _lastDebounceTime;
    static constexpr unsigned long DEBOUNCE_DELAY = 30;
    
    // Stato pulsante
    bool _currentPressed;
    bool _lastPressed;
    unsigned long _pressStartTime;
    unsigned long _lastReleaseTime;
    
    bool _clickDetected;
    bool _doubleClickDetected;
    bool _longPressDetected;
    bool _longPressHandled;
    
    static constexpr unsigned long DOUBLE_CLICK_WINDOW = 300;  // ms
    static constexpr unsigned long LONG_PRESS_DURATION = 800;   // ms
    
    // I2C communication
    bool read(uint8_t* buf, int howmany);
    bool write(uint8_t* buf, int howmany);
    bool scan();
};

#endif
