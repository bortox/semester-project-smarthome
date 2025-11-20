#ifndef DEVICES_H
#define DEVICES_H

#include "CoreSystem.h"
#include "sensors.h"

// SensorStats ottimizzato
class SensorStats {
private:
    int16_t _min;
    int16_t _max;
    int32_t _sum;
    uint16_t _count;
    
public:
    SensorStats() : _min(32767), _max(-32768), _sum(0), _count(0) {}
    
    void addValue(float value) {
        int16_t val10 = (int16_t)(value * 10);
        if (val10 < _min) _min = val10;
        if (val10 > _max) _max = val10;
        _sum += val10;
        _count++;
    }
    
    float getMin() const { return _count > 0 ? _min / 10.0f : 0; }
    float getMax() const { return _count > 0 ? _max / 10.0f : 0; }
    float getAverage() const { return _count > 0 ? (_sum / _count) / 10.0f : 0; }
};

class SimpleLight : public IDevice {
protected:
    const char* _name;
    uint8_t _pin;
    bool _state;

public:
    SimpleLight(const char* name, uint8_t pin) : _name(name), _pin(pin), _state(false) {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        DeviceRegistry::instance().registerDevice(this);
    }

    virtual ~SimpleLight() { DeviceRegistry::instance().unregisterDevice(this); }

    const char* getName() const override { return _name; }
    DeviceType getType() const override { return DeviceType::LightSimple; }
    bool isLight() const override { return true; }
    
    void update() override {}

    virtual void toggle() {
        _state = !_state;
        digitalWrite(_pin, _state ? HIGH : LOW);
        EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
    }

    bool getState() const { return _state; }
};

class DimmableLight : public SimpleLight {
private:
    uint8_t _brightness;

public:
    DimmableLight(const char* name, uint8_t pin) : SimpleLight(name, pin), _brightness(0) {}

    DeviceType getType() const override { return DeviceType::LightDimmable; }

    void setBrightness(uint8_t level) {
        _brightness = constrain(level, 0, 100);
        _state = (_brightness > 0);
        analogWrite(_pin, map(_brightness, 0, 100, 0, 255));
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, _brightness);
    }

    uint8_t getBrightness() const { return _brightness; }
    
    void toggle() override {
        setBrightness(_state ? 0 : 100);
    }
};

class TemperatureSensor : public IDevice {
private:
    const char* _name;
    float _temperature;
    unsigned long _lastRead;
    SensorStats _stats;
    LM75Sensor _lm75;

public:
    TemperatureSensor(const char* name) : _name(name), _temperature(0), _lastRead(0), _lm75(name) {
        _lm75.begin();
        DeviceRegistry::instance().registerDevice(this);
    }

    const char* getName() const override { return _name; }
    DeviceType getType() const override { return DeviceType::SensorTemperature; }
    bool isSensor() const override { return true; }

    void update() override {
        if (millis() - _lastRead > 2000) {
            _lastRead = millis();
            _temperature = _lm75.getValue();
            _stats.addValue(_temperature);
            EventSystem::instance().emit(EventType::SensorUpdated, this, (int)(_temperature * 10));
        }
    }

    float getTemperature() const { return _temperature; }
    SensorStats& getStats() { return _stats; }
};

// Factory normale (allocazione dinamica controllata)
class DeviceFactory {
public:
    static void createSimpleLight(const char* name, uint8_t pin) {
        new SimpleLight(name, pin);
    }

    static void createDimmableLight(const char* name, uint8_t pin) {
        new DimmableLight(name, pin);
    }

    static void createTemperatureSensor(const char* name) {
        new TemperatureSensor(name);
    }
};

#endif