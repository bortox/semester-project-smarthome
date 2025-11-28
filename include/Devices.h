#ifndef DEVICES_H
#define DEVICES_H

#include "CoreSystem.h"
#include "sensors.h"
#include <avr/pgmspace.h>

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

class SimpleLight : public IDevice, public IEventListener {
protected:
    const char* _name;
    uint8_t _pin;
    bool _state;

public:
    SimpleLight(const char* name, uint8_t pin) : _name(name), _pin(pin), _state(false) {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        DeviceRegistry::instance().registerDevice(this);
        
        // FIX: Registra per eventi ButtonPressed
        EventSystem::instance().addListener(this, EventType::ButtonPressed);
    }

    virtual ~SimpleLight() { 
        DeviceRegistry::instance().unregisterDevice(this);
        EventSystem::instance().removeListener(this);
    }

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
    
    // FIX: Implementa handleEvent per reagire a ButtonPressed
    void handleEvent(EventType type, IDevice* source, int value) {
        if (type == EventType::ButtonPressed && source == this) {
            toggle();
        }
    }
};

class DimmableLight : public SimpleLight {
private:
    uint8_t _brightness;
    uint8_t _lastBrightness; // Salva luminosità precedente

public:
    DimmableLight(const char* name, uint8_t pin) : SimpleLight(name, pin), _brightness(100), _lastBrightness(100) {
        // Eredita listener da SimpleLight
    }

    DeviceType getType() const override { return DeviceType::LightDimmable; }

    virtual void setBrightness(uint8_t level) {
        _brightness = constrain(level, 0, 100);
        
        // Salva luminosità se > 0
        if (_brightness > 0) {
            _lastBrightness = _brightness;
        }
        
        _state = (_brightness > 0);
        analogWrite(_pin, map(_brightness, 0, 100, 0, 255));
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, _brightness);
    }

    uint8_t getBrightness() const { return _brightness; }
    
    void toggle() override {
        if (_state) {
            // Spegnimento: salva luminosità e vai a 0
            _lastBrightness = _brightness;
            setBrightness(0);
        } else {
            // Accensione: ripristina luminosità precedente
            setBrightness(_lastBrightness > 0 ? _lastBrightness : 100);
        }
    }
    
    void handleEvent(EventType type, IDevice* source, int value) override {
        if (type == EventType::ButtonPressed && source == this) {
            toggle();
        }
    }

protected:
    uint8_t& brightness() { return _brightness; }
    uint8_t& lastBrightness() { return _lastBrightness; }
};

// --- RGB LIGHT ---

struct RGBColor { uint8_t r, g, b; };

enum class RGBPreset : uint8_t {
    WARM_WHITE, COOL_WHITE, RED, GREEN, BLUE, OCEAN
};

// Preset in PROGMEM
const RGBColor PRESET_COLORS[] PROGMEM = {
    {255, 200, 100}, // Warm
    {255, 255, 255}, // Cool
    {255, 0, 0},     // Red
    {0, 255, 0},     // Green
    {0, 0, 255},     // Blue
    {0, 150, 255}    // Ocean
};

class RGBLight : public DimmableLight {
private:
    uint8_t _pin_g, _pin_b;
    RGBColor _color;

public:
    RGBLight(const char* name, uint8_t pin_r, uint8_t pin_g, uint8_t pin_b) 
        : DimmableLight(name, pin_r), _pin_g(pin_g), _pin_b(pin_b), _color({255, 255, 255}) {
        pinMode(_pin_g, OUTPUT);
        pinMode(_pin_b, OUTPUT);
    }

    DeviceType getType() const override { return DeviceType::LightRGB; }

    void setColor(RGBColor c) {
        _color = c;
        applyColor();
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
    }

    RGBColor getColor() const { return _color; }

    void setPreset(RGBPreset preset) {
        RGBColor c;
        memcpy_P(&c, &PRESET_COLORS[(int)preset], sizeof(RGBColor));
        setColor(c);
    }

    void setBrightness(uint8_t level) override {
        brightness() = constrain(level, 0, 100);
        
        if (brightness() > 0) {
            lastBrightness() = brightness();
        }
        
        _state = (brightness() > 0);
        applyColor();
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, brightness());
    }

    void toggle() override {
        if (_state) {
            lastBrightness() = brightness();
            brightness() = 0;
            _state = false;
        } else {
            brightness() = lastBrightness() > 0 ? lastBrightness() : 100;
            _state = true;
        }
        applyColor();
        EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
    }

private:
    void applyColor() {
        if (!_state || brightness() == 0) {
            analogWrite(_pin, 0);
            analogWrite(_pin_g, 0);
            analogWrite(_pin_b, 0);
            return;
        }
        float factor = brightness() / 100.0f;
        analogWrite(_pin, _color.r * factor);
        analogWrite(_pin_g, _color.g * factor);
        analogWrite(_pin_b, _color.b * factor);
    }
};

// --- OUTSIDE LIGHT ---

enum class OutsideMode : uint8_t { OFF, ON, AUTO_LIGHT, AUTO_MOTION };

class OutsideLight : public SimpleLight {
private:
    OutsideMode _mode;
    LightSensor* _photo;
    MovementSensor* _motion;

public:
    OutsideLight(const char* name, uint8_t pin, LightSensor* photo, MovementSensor* motion)
        : SimpleLight(name, pin), _mode(OutsideMode::OFF), _photo(photo), _motion(motion) {}

    DeviceType getType() const override { return DeviceType::LightOutside; }

    void setMode(OutsideMode mode) {
        _mode = mode;
        update(); // Applica subito
    }

    void update() override {
        bool targetState = false;
        switch(_mode) {
            case OutsideMode::OFF: targetState = false; break;
            case OutsideMode::ON: targetState = true; break;
            case OutsideMode::AUTO_LIGHT:
                targetState = (_photo->getValue() < 30); // Soglia buio
                break;
            case OutsideMode::AUTO_MOTION:
                targetState = (_photo->getValue() < 30 && _motion->getValue());
                break;
        }
        
        if (targetState != _state) {
            _state = targetState;
            digitalWrite(_pin, _state ? HIGH : LOW);
            EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
        }
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

// NUOVO: Sensore di luce generico
class PhotoresistorSensor : public IDevice {
private:
    const char* _name;
    int _lightLevel;
    unsigned long _lastRead;
    LightSensor _photoSensor;

public:
    PhotoresistorSensor(const char* name, uint8_t pin) 
        : _name(name), _lightLevel(0), _lastRead(0), _photoSensor(name, pin) {
        DeviceRegistry::instance().registerDevice(this);
    }

    const char* getName() const override { return _name; }
    DeviceType getType() const override { return DeviceType::SensorLight; }
    bool isSensor() const override { return true; }

    void update() override {
        if (millis() - _lastRead > 1000) {  // Aggiorna ogni secondo
            _lastRead = millis();
            int newValue = _photoSensor.getValue();
            
            // Emetti evento solo se cambiamento significativo (>5%)
            if (abs(newValue - _lightLevel) > 5) {
                _lightLevel = newValue;
                EventSystem::instance().emit(EventType::SensorUpdated, this, _lightLevel);
            }
        }
    }

    int getValue() const { return _lightLevel; }
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
    static void createRGBLight(const char* name, uint8_t r, uint8_t g, uint8_t b) {
        new RGBLight(name, r, g, b);
    }
    static void createOutsideLight(const char* name, uint8_t pin, LightSensor* p, MovementSensor* m) {
        new OutsideLight(name, pin, p, m);
    }
    static void createPhotoresistorSensor(const char* name, uint8_t pin) {
        new PhotoresistorSensor(name, pin);
    }
};

#endif