#ifndef MODULINO_KNOB_H
#define MODULINO_KNOB_H

#include <Arduino.h>
extern "C" {
    #include "i2cmaster.h"  // Sostituisce Wire.h
}

class ModulinoKnob {
private:
    uint8_t _address;
    int16_t _lastPosition;
    bool _lastButtonState;
    uint32_t _lastReadTime;
    static constexpr uint32_t READ_INTERVAL = 10; // ms

    // Registri I2C del dispositivo
    static constexpr uint8_t REG_POSITION = 0x00;
    static constexpr uint8_t REG_BUTTON = 0x01;
    static constexpr uint8_t REG_LED_R = 0x10;
    static constexpr uint8_t REG_LED_G = 0x11;
    static constexpr uint8_t REG_LED_B = 0x12;

    // Metodi privati per comunicazione I2C a basso livello
    bool i2cWriteReg(uint8_t reg, uint8_t value);
    bool i2cReadReg(uint8_t reg, uint8_t* value);
    bool i2cReadReg16(uint8_t reg, int16_t* value);

public:
    ModulinoKnob(uint8_t address = 0x30);
    
    bool begin();
    bool update();
    
    int16_t getPosition() const { return _lastPosition; }
    bool isButtonPressed() const { return _lastButtonState; }
    
    bool setLED(uint8_t r, uint8_t g, uint8_t b);
    bool reset();
};

#endif // MODULINO_KNOB_H
