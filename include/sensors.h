#pragma once
#include <Arduino.h>
#include "DebugConfig.h"
#include "i2cmaster.h"
#include "MemoryMonitor.h"

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

// ===== VIRTUAL SENSORS =====

/**
 * @class RamUsageSensor
 * @brief Low-level RAM monitoring sensor
 * 
 * Measures free memory between heap and stack using AVR memory layout.
 */
class RamUsageSensor : public Sensor<int16_t> {
public:
    RamUsageSensor(const char* name) : Sensor<int16_t>(name) {}
    
    /**
     * @brief Gets current free RAM in bytes
     * @return Free memory bytes
     */
    int16_t getValue() const override {
        return getFreeMemory();
    }
};

/**
 * @class VccSensor
 * @brief Low-level VCC voltage sensor
 * 
 * Uses internal 1.1V bandgap reference to measure supply voltage.
 * Compatible with ATmega328P and ATmega32U4.
 */
class VccSensor : public Sensor<int16_t> {
public:
    VccSensor(const char* name) : Sensor<int16_t>(name) {}
    
    /**
     * @brief Reads VCC using internal 1.1V reference
     * @return VCC in millivolts (e.g., 5000 for 5.0V)
     */
    int16_t getValue() const override {
        // Save current ADMUX state
        uint8_t savedADMUX = ADMUX;
        
        // Set reference to AVcc and measure internal 1.1V bandgap
        #if defined(__AVR_ATmega32U4__)
            // ATmega32U4: MUX[4:0] = 11110 for bandgap
            ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
        #else
            // ATmega328P: MUX[3:0] = 1110 for bandgap
            ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
        #endif
        
        // Wait for voltage to stabilize
        delayMicroseconds(250);
        
        // Start conversion
        ADCSRA |= _BV(ADSC);
        
        // Wait for conversion to complete
        while (bit_is_set(ADCSRA, ADSC));
        
        // Read ADC value (low byte first for atomic read)
        uint8_t low = ADCL;
        uint8_t high = ADCH;
        uint16_t adcValue = (high << 8) | low;
        
        // Restore ADMUX
        ADMUX = savedADMUX;
        
        // Calculate VCC: VCC = 1.1V * 1024 / ADC
        // Using 1125300L = 1.1 * 1023 * 1000 for millivolt result
        if (adcValue == 0) return 0;
        return (int16_t)(1125300L / adcValue);
    }
};

/**
 * @class LoopTimeSensor
 * @brief Low-level loop execution time sensor
 * 
 * Tracks maximum loop time within measurement windows.
 * External code must call registerTime() at end of each loop iteration.
 */
class LoopTimeSensor : public Sensor<int16_t> {
private:
    static LoopTimeSensor* _instance;
    uint16_t _currentMax;
    uint16_t _reportedMax;
    unsigned long _windowStart;
    static constexpr unsigned long WINDOW_MS = 1000;

public:
    LoopTimeSensor(const char* name) : Sensor<int16_t>(name), _currentMax(0), _reportedMax(0), _windowStart(0) {
        _instance = this;
        _windowStart = millis();
    }
    
    /**
     * @brief Registers a loop iteration time
     * @param microseconds Duration of the loop iteration
     * 
     * Call this from main loop. Tracks maximum value within current window.
     */
    static void registerTime(uint16_t microseconds) {
        if (_instance && microseconds > _instance->_currentMax) {
            _instance->_currentMax = microseconds;
        }
    }
    
    /**
     * @brief Gets the maximum loop time from last window
     * @return Loop time in microseconds
     */
    int16_t getValue() const override {
        return (int16_t)_reportedMax;
    }
    
    /**
     * @brief Updates the measurement window
     * 
     * Call periodically to rotate windows and update reported value.
     */
    void updateWindow() {
        unsigned long now = millis();
        if (now - _windowStart >= WINDOW_MS) {
            _reportedMax = _currentMax;
            _currentMax = 0;
            _windowStart = now;
        }
    }
};