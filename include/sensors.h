#pragma once
#include <Arduino.h>
#include "DebugConfig.h"
extern "C" { 
    #include "i2cmaster.h"
 }

// BASE SENSOR
template<typename T>
class Sensor {
protected:
    const char* _name;
public:
    Sensor(const char* name) : _name(name) {}
    virtual T getValue() const = 0;
};

// LM75 TEMP SENSOR (NO FLOATS - Returns decicelsius)
#ifndef LM75_ADR
#define LM75_ADR 0x90
#endif

class LM75Sensor : public Sensor<int16_t> {
public:
    LM75Sensor(const char* name) : Sensor<int16_t>(name) {}
    
    void begin() {
#if DEBUG_I2C
        digitalWrite(LED_BUILTIN, HIGH);
#endif
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x01);
        i2c_write(0x00);
        i2c_stop();
#if DEBUG_I2C
        digitalWrite(LED_BUILTIN, LOW);
#endif
    }
    
    /**
     * @brief Gets temperature in decicelsius (tenths of degree)
     * @return Temperature * 10 (e.g., 205 = 20.5°C)
     * 
     * LM75 resolution is 0.125°C (1/8), we need 0.1°C (1/10)
     * Formula: decicelsius = (raw_value * 5) >> 2
     * Explanation: raw * 0.125 * 10 = raw * 1.25 = raw * 5/4
     */
    int16_t getValue() const override {
#if DEBUG_I2C
        digitalWrite(LED_BUILTIN, HIGH);
#endif
        uint16_t temp_data;
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x00);
        i2c_rep_start(LM75_ADR + I2C_READ);
        uint8_t high_byte = i2c_readAck();
        uint8_t low_byte = i2c_readNak();
        i2c_stop();
#if DEBUG_I2C
        digitalWrite(LED_BUILTIN, LOW);
#endif
        
        temp_data = (high_byte << 8) | low_byte;
        temp_data = temp_data >> 5;
        
        // Handle negative temperatures
        if (high_byte & 0x80) {
            temp_data |= 0xF800;
            int16_t raw = static_cast<int16_t>(temp_data);
            // raw * 0.125 * 10 = (raw * 5) / 4
            return (raw * 5) >> 2;
        }
        
        // Positive temperature: (raw * 5) / 4
        return (static_cast<int16_t>(temp_data) * 5) >> 2;
    }
};

// LIGHT SENSOR
class LightSensor : public Sensor<int> {
private:
    uint8_t _pin;
    int _rawMin;  // Calibrated dark value
    int _rawMax;  // Calibrated bright value

public:
    LightSensor(const char* name, uint8_t pin) 
        : Sensor<int>(name), _pin(pin), _rawMin(1023), _rawMax(0) {
        pinMode(_pin, INPUT_PULLUP);
    }
    
    int getValue() const override { 
        int raw = getRaw();
        // Handle inverted range (PULLUP: dark=1023, bright=0)
        return map(raw, _rawMin, _rawMax, 0, 100); 
    }
    
    int getRaw() const { return analogRead(_pin); }
    
    void setRawMin(int val) { _rawMin = constrain(val, 0, 1023); }
    void setRawMax(int val) { _rawMax = constrain(val, 0, 1023); }
    
    int getRawMin() const { return _rawMin; }
    int getRawMax() const { return _rawMax; }
};

// MOTION SENSOR
class MovementSensor : public Sensor<bool> {
private:
    uint8_t _pin;
public:
    MovementSensor(const char* name, uint8_t pin) : Sensor<bool>(name), _pin(pin) {
        pinMode(_pin, INPUT);
    }
    bool getValue() const override { return digitalRead(_pin); }
};