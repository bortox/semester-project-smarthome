#pragma once
#include "i2cmaster.h" // La tua libreria per la comunicazione I2C
#include <sensorborto.h>
// Il define dell'indirizzo può stare qui o essere passato al costruttore
#ifndef LM75_ADR
#define LM75_ADR 0x90 // Indirizzo base LM75 (A2=A1=A0=GND)
#endif

class LM75Sensor : public Sensor<float> {
private:
    const char* _name;
    // Potremmo anche rendere l'indirizzo un membro se avessimo più sensori
    // uint8_t _address; 

public:
    LM75Sensor(const char* name) : Sensor<float>(name), _name(name) {
        // La logica di init va nel costruttore
        i2c_start_wait(LM75_ADR + I2C_WRITE);
        i2c_write(0x01); // Punta al registro di configurazione
        i2c_write(0x00); // Modalità normale
        i2c_stop();
    }
    
    const char* getName() const { return _name; }

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
             // Converti da 11-bit a 16-bit estendendo il segno
             temp_data |= 0xF800; 
             // Ora il cast a signed int gestirà il complemento a due
             return static_cast<int16_t>(temp_data) * 0.125f;
        } else { // Numero positivo
             return temp_data * 0.125f;
        }
    }
};