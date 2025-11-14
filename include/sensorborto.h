// --- SENSOR CLASSES ---

// Classe base per tutti i sensori
#include "Arduino.h"

template<typename T>
class Sensor {
protected:
    const char* _name;
    virtual T getValue() const = 0;
public:
    Sensor(const char* name) : _name(name) {}
    const char* getName() const { return _name; }
};

// include LM75Sensor<float>
#include "lm75.h"


// Sensore di Luce (Fotoresistore)
class LightSensor : public Sensor<int> {
private:
    uint8_t _pin;
public:
    LightSensor(const char* name, uint8_t analog_pin) : Sensor<int>(name), _pin(analog_pin) {
        pinMode(_pin, INPUT); // optional, analog pins are input by default
    }
    int getLightLevel() const {
        return analogRead(_pin);
    }
    int getValue() const override {
        return map(getLightLevel(), 0, 1023, 0, 100);
    }
};

// Sensore di movimento HCSR501
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