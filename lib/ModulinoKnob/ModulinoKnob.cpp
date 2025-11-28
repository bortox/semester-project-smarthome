#include "ModulinoKnob.h"

ModulinoKnob::ModulinoKnob(uint8_t address)
    : _address(address), _lastPosition(0), _lastButtonState(false), _lastReadTime(0) {
}

bool ModulinoKnob::begin() {
    // i2c_init() dovrebbe essere già chiamato nel main (una sola volta)
    // Verifica la presenza del dispositivo
    uint8_t i2cAddr = (_address << 1); // Indirizzo I2C con bit R/W
    
    if (i2c_start(i2cAddr + I2C_WRITE) != 0) {
        i2c_stop();
        return false; // Dispositivo non risponde
    }
    i2c_stop();
    
    return true;
}

bool ModulinoKnob::update() {
    uint32_t now = millis();
    if (now - _lastReadTime < READ_INTERVAL) {
        return true; // Troppo presto per leggere
    }
    _lastReadTime = now;

    // Leggi posizione encoder (16-bit signed)
    int16_t position;
    if (!i2cReadReg16(REG_POSITION, &position)) {
        return false;
    }
    _lastPosition = position;

    // Leggi stato pulsante
    uint8_t buttonState;
    if (!i2cReadReg(REG_BUTTON, &buttonState)) {
        return false;
    }
    _lastButtonState = (buttonState != 0);

    return true;
}

bool ModulinoKnob::setLED(uint8_t r, uint8_t g, uint8_t b) {
    if (!i2cWriteReg(REG_LED_R, r)) return false;
    if (!i2cWriteReg(REG_LED_G, g)) return false;
    if (!i2cWriteReg(REG_LED_B, b)) return false;
    return true;
}

bool ModulinoKnob::reset() {
    // Implementa reset se supportato dal dispositivo
    // Per ora ritorna true
    return true;
}

// ==================== Metodi privati I2C ====================

bool ModulinoKnob::i2cWriteReg(uint8_t reg, uint8_t value) {
    uint8_t i2cAddr = (_address << 1);
    
    if (i2c_start(i2cAddr + I2C_WRITE) != 0) {
        i2c_stop();
        return false;
    }
    
    if (i2c_write(reg) != 0) {
        i2c_stop();
        return false;
    }
    
    if (i2c_write(value) != 0) {
        i2c_stop();
        return false;
    }
    
    i2c_stop();
    return true;
}

bool ModulinoKnob::i2cReadReg(uint8_t reg, uint8_t* value) {
    uint8_t i2cAddr = (_address << 1);
    
    // Write register address
    if (i2c_start(i2cAddr + I2C_WRITE) != 0) {
        i2c_stop();
        return false;
    }
    
    if (i2c_write(reg) != 0) {
        i2c_stop();
        return false;
    }
    
    // Repeated start per lettura
    if (i2c_rep_start(i2cAddr + I2C_READ) != 0) {
        i2c_stop();
        return false;
    }
    
    *value = i2c_readNak(); // Leggi byte con NACK (ultimo byte)
    i2c_stop();
    
    return true;
}

bool ModulinoKnob::i2cReadReg16(uint8_t reg, int16_t* value) {
    uint8_t i2cAddr = (_address << 1);
    
    // Write register address
    if (i2c_start(i2cAddr + I2C_WRITE) != 0) {
        i2c_stop();
        return false;
    }
    
    if (i2c_write(reg) != 0) {
        i2c_stop();
        return false;
    }
    
    // Repeated start per lettura
    if (i2c_rep_start(i2cAddr + I2C_READ) != 0) {
        i2c_stop();
        return false;
    }
    
    // Leggi 2 bytes (little-endian: LSB first)
    uint8_t lowByte = i2c_readAck();
    uint8_t highByte = i2c_readNak();
    i2c_stop();
    
    *value = (int16_t)((highByte << 8) | lowByte);
    
    return true;
}
