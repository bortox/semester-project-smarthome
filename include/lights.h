#pragma once
#include <Arduino.h>
#include "sensors.h" // Includiamo i sensori per AutomaticLight

// --- UTILS E CLASSI DI SUPPORTO ---
constexpr int USER_SCALE_MIN = 0;
constexpr int USER_SCALE_MAX = 100;
constexpr int PWM_SCALE_MIN = 0;
constexpr int PWM_SCALE_MAX = 255;

namespace BrightnessUtils {
    static int toPWM(int value100) {
        return constrain(map(value100, USER_SCALE_MIN, USER_SCALE_MAX, PWM_SCALE_MIN, PWM_SCALE_MAX), PWM_SCALE_MIN, PWM_SCALE_MAX);
    }
    static int toUser(int value255) {
        return constrain(map(value255, PWM_SCALE_MIN, PWM_SCALE_MAX, USER_SCALE_MIN, USER_SCALE_MAX), USER_SCALE_MIN, USER_SCALE_MAX);
    }
}

class RGBColor {
public:
    uint8_t r, g, b;
    RGBColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
    
    static RGBColor Off() { return RGBColor(0, 0, 0); }
    static RGBColor White() { return RGBColor(255, 255, 255); }
    static RGBColor WarmWhite() { return RGBColor(255, 204, 153); }
    static RGBColor OceanBlue() { return RGBColor(0, 119, 190); }
    static RGBColor Red() { return RGBColor(255, 0, 0); }
};

// --- GERARCHIA CLASSI LUCI ---

// Classe Base per tutte le luci
class Light {
protected:
    const char* _name;
    bool _status = false;
public:
    Light(const char* name) : _name(name) {}
    virtual ~Light() {}
    const char* getName() const { return _name; }
    bool getStatus() const { return _status; }
    virtual void setStatus(bool status) = 0;
    void toggle() { setStatus(!_status); }
};

// Luce Semplice ON/OFF
class SimpleLight : public Light {
private:
    uint8_t _pin;
public:
    SimpleLight(const char* name, uint8_t pin) : Light(name), _pin(pin) {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
    }
    void setStatus(bool status) override {
        _status = status;
        digitalWrite(_pin, _status ? HIGH : LOW);
    }
};

// Luce Dimmerabile
class DimmableLight : public Light {
private:
    uint8_t _pin;
    int _current_pwm = 0;
    int _last_user_percent = 100;
public:
    DimmableLight(const char* name, uint8_t pin) : Light(name), _pin(pin) {
        pinMode(_pin, OUTPUT);
        analogWrite(_pin, 0);
    }

    void setBrightness(int user_percent) {
        _current_pwm = BrightnessUtils::toPWM(user_percent);
        if (user_percent > 0) {
            _last_user_percent = user_percent;
            _status = true;
        } else {
            _status = false;
        }
        analogWrite(_pin, _current_pwm);
    }
    
    int getBrightness() const {
        return BrightnessUtils::toUser(_current_pwm);
    }
    
    void setStatus(bool status) override {
        setBrightness(status ? _last_user_percent : 0);
    }
};

// Luce RGB
class RGBLight : public Light {
private:
    DimmableLight _channel_r, _channel_g, _channel_b;
    RGBColor _current_color = RGBColor::White();
    int _master_brightness = 100; // Master DIM (0-100)

    // Metodo privato per applicare colore e luminosità
    void _applyColorAndBrightness() {
        if (!_status) {
            _channel_r.setBrightness(0);
            _channel_g.setBrightness(0);
            _channel_b.setBrightness(0);
            return;
        }
        // Applica il master brightness al colore corrente
        uint8_t r = (_current_color.r * _master_brightness) / 100;
        uint8_t g = (_current_color.g * _master_brightness) / 100;
        uint8_t b = (_current_color.b * _master_brightness) / 100;

        _channel_r.setBrightness(BrightnessUtils::toUser(r));
        _channel_g.setBrightness(BrightnessUtils::toUser(g));
        _channel_b.setBrightness(BrightnessUtils::toUser(b));
    }

public:
    RGBLight(const char* name, uint8_t pin_r, uint8_t pin_g, uint8_t pin_b)
        : Light(name),
          _channel_r("Red", pin_r), _channel_g("Green", pin_g), _channel_b("Blue", pin_b)
    {}

    // Imposta il colore (salva il valore "puro")
    void setColor(const RGBColor& color) {
        _current_color = color;
        _status = (color.r > 0 || color.g > 0 || color.b > 0);
        _applyColorAndBrightness();
    }

    // Imposta il DIM generale
    void setBrightness(int percent) {
        _master_brightness = constrain(percent, 0, 100);
        // Se la luce è accesa, riapplica la luminosità
        if (_status) {
            _applyColorAndBrightness();
        }
    }

    int getBrightness() const { return _master_brightness; }
    
    void setStatus(bool status) override {
        _status = status;
        if (_status) {
            // Se accendo, ripristino l'ultimo colore e luminosità
            _applyColorAndBrightness();
        } else {
            // Se spengo, metto i canali a zero
            _channel_r.setBrightness(0);
            _channel_g.setBrightness(0);
            _channel_b.setBrightness(0);
        }
    }
};

// Luce Automatica per Esterni
class AutomaticLight : public Light {
public:
    enum Mode { MODE_OFF, MODE_ON, MODE_AUTO_LIGHT, MODE_AUTO_MOTION };

private:
    SimpleLight _light; // Usa composizione: contiene una luce fisica
    LightSensor& _lightSensor;
    MovementSensor& _movementSensor;
    Mode _currentMode = MODE_AUTO_LIGHT;
    
    unsigned long _motion_trigger_time = 0;
    const unsigned long MOTION_LIGHT_DURATION = 30000; // 30 secondi

public:
    // Variabili pubbliche per binding con menu
    int light_threshold = 50; // Soglia in % (0-100) per accensione automatica

    AutomaticLight(const char* name, uint8_t light_pin, LightSensor& ls, MovementSensor& ms)
        : Light(name), 
          _light(name, light_pin), 
          _lightSensor(ls), 
          _movementSensor(ms)
    {}

    void setMode(Mode mode) {
        _currentMode = mode;
        update(); // Applica subito la logica del nuovo modo
    }
    Mode getMode() const { return _currentMode; }

    // Implementa setStatus per coerenza con le altre luci
    void setStatus(bool status) override {
        setMode(status ? MODE_ON : MODE_OFF);
    }

    // Il cuore della logica, da chiamare nel loop() principale
    void update() {
        switch (_currentMode) {
            case MODE_ON:
                _light.setStatus(true);
                break;
            case MODE_OFF:
                _light.setStatus(false);
                break;
            case MODE_AUTO_LIGHT:
                // Se è buio, accendi. Altrimenti, spegni.
                _light.setStatus(_lightSensor.getValue() < light_threshold);
                break;
            case MODE_AUTO_MOTION:
                // Se è buio
                if (_lightSensor.getValue() < light_threshold) {
                    // E se c'è movimento
                    if (_movementSensor.getValue()) {
                        _light.setStatus(true);
                        _motion_trigger_time = millis(); // Resetta il timer
                    } else {
                        // Se non c'è movimento, controlla se è passato il tempo
                        if (millis() - _motion_trigger_time > MOTION_LIGHT_DURATION) {
                            _light.setStatus(false);
                        }
                    }
                } else {
                    // Se c'è luce, la luce deve essere spenta
                    _light.setStatus(false);
                }
                break;
        }
        // Aggiorna lo stato "pubblico" della classe base
        _status = _light.getStatus();
    }
};