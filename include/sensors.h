/**
 * @file sensors.h
 * @brief Low-level sensor abstractions and hardware drivers
 * @author Andrea Bortolotti
 * @version 2.0
 * 
 * @details Provides template-based sensor interface and concrete
 * implementations for various sensor types:
 * - LM75 I2C temperature sensor
 * - Analog photoresistor
 * - HC-SR501 PIR motion sensor
 * - Virtual sensors (RAM, VCC, Loop Time)
 * 
 * @note This file must remain header-only due to template classes.
 * 
 * @ingroup Devices
 */
#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "DebugConfig.h"
#include "i2cmaster.h"
#include "MemoryMonitor.h"

/**
 * @class Sensor
 * @brief Abstract template base class for all sensors
 * @tparam T Return type for sensor readings
 * @ingroup Devices
 * 
 * @details Provides common interface for sensor value retrieval.
 * Concrete implementations must override getValue().
 */
template<typename T>
class Sensor {
public:
    /**
     * @brief Default constructor
     */
    Sensor() {} 
    
    /**
     * @brief Virtual destructor
     */
    virtual ~Sensor() {}
    
    /**
     * @brief Gets current sensor value
     * @return Sensor reading of type T
     */
    virtual T getValue() const = 0;
};

/**
 * @brief I2C address for LM75 temperature sensor
 * @details Default address with A0-A2 grounded
 */
#ifndef LM75_ADR
#define LM75_ADR 0x90
#endif

/**
 * @class LM75Sensor
 * @brief LM75 I2C temperature sensor driver (NO FLOATS)
 * @ingroup Devices
 * 
 * @details Communicates with LM75 via I2C to read temperature.
 * Returns temperature in decicelsius (tenths of degree) to avoid
 * floating-point operations on AVR.
 */
class LM75Sensor : public Sensor<int16_t> {
public:   
    /**
     * @brief Constructor
     */
    LM75Sensor() : Sensor<int16_t>() {}  
    
    /**
     * @brief Initializes the LM75 sensor
     * @details Configures sensor for continuous conversion mode
     */
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
     * @brief Gets temperature in decicelsius
     * @return Temperature × 10 (e.g., 205 = 20.5°C)
     * 
     * @details LM75 resolution is 0.125°C (1/8), converted to 0.1°C (1/10).
     * Formula: decicelsius = (raw_value × 5) >> 2
     * Explanation: raw × 0.125 × 10 = raw × 1.25 = raw × 5/4
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
        
        if (high_byte & 0x80) {
            temp_data |= 0xF800;
            int16_t raw = static_cast<int16_t>(temp_data);
            return (raw * 5) >> 2;
        }
        
        return (static_cast<int16_t>(temp_data) * 5) >> 2;
    }
};

/**
 * @class LightSensor
 * @brief Analog photoresistor sensor with calibration
 * @ingroup Devices
 * 
 * @details Reads analog voltage from photoresistor and maps to
 * percentage based on calibrated min/max values.
 */
class LightSensor : public Sensor<int> {
private:
    uint8_t _pin;   ///< Arduino analog pin number
    int _rawMin;    ///< Calibrated dark value (0-1023)
    int _rawMax;    ///< Calibrated bright value (0-1023)

public:
    /**
     * @brief Constructor
     * @param pin Arduino analog pin number
     */
    explicit LightSensor(uint8_t pin) 
        : Sensor<int>(), _pin(pin), _rawMin(0), _rawMax(1023) {
        pinMode(_pin, INPUT);
    }
    
    /**
     * @brief Gets light level as percentage
     * @return Light level 0-100%
     */
    int getValue() const override { 
        int raw = getRaw();
        return map(raw, _rawMin, _rawMax, 0, 100); 
    }
    
    /**
     * @brief Gets raw ADC reading
     * @return Raw value 0-1023
     */
    int getRaw() const { return analogRead(_pin); }
    
    /**
     * @brief Sets minimum calibration value
     * @param val Dark reference value (0-1023)
     */
    void setRawMin(int val) { _rawMin = constrain(val, 0, 1023); }
    
    /**
     * @brief Sets maximum calibration value
     * @param val Bright reference value (0-1023)
     */
    void setRawMax(int val) { _rawMax = constrain(val, 0, 1023); }
    
    /**
     * @brief Gets minimum calibration value
     * @return Dark reference value
     */
    int getRawMin() const { return _rawMin; }
    
    /**
     * @brief Gets maximum calibration value
     * @return Bright reference value
     */
    int getRawMax() const { return _rawMax; }
};

/**
 * @class MovementSensor
 * @brief HC-SR501 PIR motion sensor driver
 * @ingroup Devices
 * 
 * @details Reads digital output from PIR sensor to detect motion.
 */
class MovementSensor : public Sensor<bool> {
private:
    uint8_t _pin;  ///< Arduino digital pin number
    
public:
    /**
     * @brief Constructor
     * @param pin Arduino digital pin number
     */
    explicit MovementSensor(uint8_t pin) : Sensor<bool>(), _pin(pin) {
        pinMode(_pin, INPUT);
    }
    
    /**
     * @brief Gets motion detection state
     * @return true if motion detected, false otherwise
     */
    bool getValue() const override { return digitalRead(_pin); }
};

/**
 * @class RamUsageSensor
 * @brief Virtual sensor for RAM monitoring
 * @ingroup Devices
 * 
 * @details Measures free memory between heap and stack using AVR memory layout.
 */
class RamUsageSensor : public Sensor<int16_t> {
public:
    /**
     * @brief Constructor
     */
    RamUsageSensor() : Sensor<int16_t>() {}
    
    /**
     * @brief Gets current free RAM
     * @return Free memory in bytes
     */
    int16_t getValue() const override {
        return getFreeMemory();
    }
};

/**
 * @class VccSensor
 * @brief Virtual sensor for supply voltage monitoring
 * @ingroup Devices
 * 
 * @details Uses internal 1.1V bandgap reference to measure supply voltage.
 * Compatible with ATmega328P and ATmega32U4.
 */
class VccSensor : public Sensor<int16_t> {
public:
    /**
     * @brief Constructor
     */
    VccSensor() : Sensor<int16_t>() {}
    
    /**
     * @brief Reads VCC using internal 1.1V reference
     * @return VCC in millivolts (e.g., 5000 for 5.0V)
     */
    int16_t getValue() const override {
        uint8_t savedADMUX = ADMUX;
        
#if defined(__AVR_ATmega32U4__)
        ADMUX = static_cast<uint8_t>(_BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1));
#else
        ADMUX = static_cast<uint8_t>(_BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1));
#endif
        
        delayMicroseconds(250);
        ADCSRA |= static_cast<uint8_t>(_BV(ADSC));
        while (bit_is_set(ADCSRA, ADSC));
        
        uint8_t low = ADCL;
        uint8_t high = ADCH;
        uint16_t adcValue = static_cast<uint16_t>((high << 8) | low);
        
        ADMUX = savedADMUX;
        
        if (adcValue == 0) return 0;
        return static_cast<int16_t>(1125300L / adcValue);
    }
};

/**
 * @class LoopTimeSensor
 * @brief Virtual sensor for loop execution time monitoring
 * @ingroup Devices
 * 
 * @details Tracks maximum loop time within measurement windows.
 * External code must call registerTime() at end of each loop iteration.
 */
class LoopTimeSensor : public Sensor<int16_t> {
private:
    static LoopTimeSensor* _instance;  ///< Singleton instance pointer
    uint16_t _currentMax;              ///< Current window maximum
    uint16_t _reportedMax;             ///< Last window maximum (reported)
    unsigned long _windowStart;        ///< Window start timestamp
    static constexpr unsigned long WINDOW_MS = 1000;  ///< Window duration

public:
    /**
     * @brief Constructor
     * @details Sets singleton instance pointer for static access
     */
    LoopTimeSensor() : Sensor<int16_t>(), _currentMax(0), _reportedMax(0), _windowStart(0) {
        _instance = this;
        _windowStart = millis();
    }
    
    /**
     * @brief Registers a loop iteration time
     * @param microseconds Duration of the loop iteration in µs
     * 
     * @details Call this from main loop. Tracks maximum value within current window.
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
        return static_cast<int16_t>(_reportedMax);
    }
    
    /**
     * @brief Updates the measurement window
     * @details Call periodically to rotate windows and update reported value.
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

#endif