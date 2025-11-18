#pragma once
#include <Arduino.h>

// --- UTILS ---
namespace BrightnessUtils {
    static int toPWM(int value100) {
        return constrain(map(value100, 0, 100, 0, 255), 0, 255);
    }
    static int toUser(int value255) {
        return constrain(map(value255, 0, 255, 0, 100), 0, 100);
    }
}

// --- CLASSE COLORE RGB ---
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
    int _master_brightness = 100;

    void _applyColorAndBrightness() {
        if (!_status) {
            _channel_r.setBrightness(0);
            _channel_g.setBrightness(0);
            _channel_b.setBrightness(0);
            return;
        }
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
          _channel_r("Red", pin_r), _channel_g("Green", pin_g), _channel_b("Blue", pin_b) {}

    void setColor(const RGBColor& color) {
        _current_color = color;
        _status = (color.r > 0 || color.g > 0 || color.b > 0);
        _applyColorAndBrightness();
    }

    void setBrightness(int percent) {
        _master_brightness = constrain(percent, 0, 100);
        if (_status) {
            _applyColorAndBrightness();
        }
    }

    int getBrightness() const { return _master_brightness; }
    
    void setStatus(bool status) override {
        _status = status;
        _applyColorAndBrightness();
    }
};