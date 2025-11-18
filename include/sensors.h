#pragma once
#include <Arduino.h>
extern "C" {
  #include "i2cmaster.h"
}

// --- CLASSE BASE PER SENSORI ---
template<typename T>
class Sensor {
protected:
    const char* _name;
public:
    Sensor(const char* name) : _name(name) {}
    virtual ~Sensor() {}
    const char* getName() const { return _name; }
    virtual T getValue() const = 0;
};

// --- IMPLEMENTAZIONI SPECIFICHE ---
#ifndef LM75_ADR
#define LM75_ADR 0x90
#endif

class LM75Sensor : public Sensor<float> {
public:
    LM75Sensor(const char* name) : Sensor<float>(name) {}

    void begin() {
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x01);
        i2c_write(0x00);
        i2c_stop();
    }
    
    float getValue() const override {
        unsigned int temp_data;
        
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x00);
        i2c_rep_start(LM75_ADR + I2C_READ);
        
        uint8_t high_byte = i2c_readAck();
        uint8_t low_byte = i2c_readNak();
        i2c_stop();

        temp_data = (high_byte << 8) | low_byte;
        temp_data = temp_data >> 5;

        if (high_byte & 0x80) {
             temp_data |= 0xF800; 
             return static_cast<int16_t>(temp_data) * 0.125f;
        } else {
             return temp_data * 0.125f;
        }
    }
};

// Sensore di Luce
class LightSensor : public Sensor<int> {
private:
    uint8_t _pin;
public:
    LightSensor(const char* name, uint8_t analog_pin) : Sensor<int>(name), _pin(analog_pin) {
        pinMode(_pin, INPUT);
    }
    
    int getValue() const override {
        return map(analogRead(_pin), 0, 1023, 0, 100);
    }
};

// Sensore di Movimento
class MovementSensor : public Sensor<bool> {
private:
    uint8_t _pin;
public:
    MovementSensor(const char* name, uint8_t pin) : Sensor<bool>(name), _pin(pin) {
        pinMode(_pin, INPUT);
    }

    bool getValue() const override {
        return digitalRead(_pin);
    }
};