#pragma once
#include <Arduino.h>
#include "i2cmaster.h" // La tua libreria per I2C

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

// Sensore di Temperatura LM75 via I2C
#ifndef LM75_ADR
#define LM75_ADR 0x90
#endif

class LM75Sensor : public Sensor<float> {
public:
    LM75Sensor(const char* name) : Sensor<float>(name) {
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x01); // Punta al registro di configurazione
        i2c_write(0x00); // Modalità normale
        i2c_stop();
    }

    float getValue() const override {
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x00); // Punta al registro di temperatura
        i2c_rep_start(LM75_ADR + I2C_READ);
        
        uint8_t high_byte = i2c_readAck();
        uint8_t low_byte = i2c_readNak();
        i2c_stop();

        uint16_t temp_data = (high_byte << 8) | low_byte;
        temp_data = temp_data >> 5;

        if (high_byte & 0x80) { // Numero negativo (complemento a due)
             temp_data |= 0xF800; 
             return static_cast<int16_t>(temp_data) * 0.125f;
        } else {
             return temp_data * 0.125f;
        }
    }
};

// Sensore di Luce (Fotoresistore)
class LightSensor : public Sensor<int> {
private:
    uint8_t _pin;
public:
    LightSensor(const char* name, uint8_t analog_pin) : Sensor<int>(name), _pin(analog_pin) {
        pinMode(_pin, INPUT);
    }
    
    // Metodo helper per avere il valore RAW se necessario
    int getRawValue() const {
        return analogRead(_pin);
    }

    // Ritorna un valore in percentuale (0-100)
    int getValue() const override {
        return map(getRawValue(), 0, 1023, 0, 100);
    }
};

// Sensore di Movimento (PIR)
class MovementSensor : public Sensor<bool> {
private:
    uint8_t _pin;
public:
    MovementSensor(const char* name, uint8_t digital_pin) : Sensor<bool>(name), _pin(digital_pin) {
        pinMode(_pin, INPUT);
    }

    bool getValue() const override {
        return digitalRead(_pin);
    }
};