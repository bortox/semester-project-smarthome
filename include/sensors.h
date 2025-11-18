#pragma once
#include <Arduino.h>
extern "C" {
  #include "i2cmaster.h"
}

// --- CLASSE BASE PER SENSORI ---
template<typename T>
class Sensor {
protected:
    String _name;
public:
    Sensor(const String& name) : _name(name) {}
    virtual ~Sensor() {}
    const String& getName() const { return _name; }
    virtual T getValue() const = 0;
};
// ... (la classe base Sensor e le altre rimangono uguali) ...

// --- IMPLEMENTAZIONI SPECIFICHE ---

// Sensore di Temperatura LM75 via I2C (versione con Wire.h)
// L'indirizzo I2C standard per la libreria Wire è a 7 bit. 
// 0x90 (8-bit) diventa 0x48 (7-bit).
#ifndef LM75_ADDRESS
#define LM75_ADDRESS 0x48 
#endif

// Indirizzo a 8 bit, come richiesto dalla libreria di Fleury
#ifndef LM75_ADR
#define LM75_ADR 0x90 // Indirizzo base LM75 (A2=A1=A0=GND)
#endif

class LM75Sensor : public Sensor<float> {
public:
    LM75Sensor(const String& name) : Sensor<float>(name) {
    }

    void begin() {
        // La logica di init del sensore (non del bus) NON va nel costruttore
        // altrimenti lo inizializzo prima del bus I2C
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x01); // Punta al registro di configurazione
        i2c_write(0x00); // Modalità normale
        i2c_stop();
    }
    
    float getValue() const override {
        unsigned int temp_data;
        
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x00); // Punta al registro di temperatura
        i2c_rep_start(LM75_ADR + I2C_READ);
        
        uint8_t high_byte = i2c_readAck();
        uint8_t low_byte = i2c_readNak();
        i2c_stop();

        // La logica di conversione rimane identica
        temp_data = (high_byte << 8) | low_byte;
        temp_data = temp_data >> 5;

        // Gestione del segno (2's complement)
        if (high_byte & 0x80) { // Numero negativo
             temp_data |= 0xF800; 
             return static_cast<int16_t>(temp_data) * 0.125f;
        } else { // Numero positivo
             return temp_data * 0.125f;
        }
    }
};

// Sensore di Luce (Fotoresistore)
class LightSensor : public Sensor<int> {
private:
    uint8_t _pin;
public:
    LightSensor(const String& name, uint8_t analog_pin) : Sensor<int>(name), _pin(analog_pin) {
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
// IN TEORIA, UN BOTTONE HA LA STESSA CLASSE
class MovementSensor : public Sensor<bool> {
private:
    uint8_t _pin;
public:
    MovementSensor(const String& name, uint8_t pin) : Sensor<bool>(name), _pin(pin) {
        pinMode(_pin, INPUT);
    }

    bool getValue() const override {
        return digitalRead(_pin);
    }
};