#pragma once
#include <Arduino.h>


// Conversion utils for light from user input on screen to PWM analogwrite value.
constexpr int USER_SCALE_MIN = 1;
constexpr int USER_SCALE_MAX = 100;
constexpr int PWM_SCALE_MIN = 1;
constexpr int PWM_SCALE_MAX = 255;

namespace BrightnessUtils {
    static int toPWM(int value100) {
        return constrain(map(value100, USER_SCALE_MIN, USER_SCALE_MAX, PWM_SCALE_MIN, PWM_SCALE_MAX), PWM_SCALE_MIN, PWM_SCALE_MAX);
    }
    static int toUser(int value255) {
        return constrain(map(value255, PWM_SCALE_MIN, PWM_SCALE_MAX, USER_SCALE_MIN, USER_SCALE_MAX), USER_SCALE_MIN, USER_SCALE_MAX);
    }
}

// Classe contenitore per i valori di colore RAW
class RGBColor {
public:
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    RGBColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) 
        : r(red), g(green), b(blue) {}
    
    static RGBColor Off() { return RGBColor(0, 0, 0); }
    static RGBColor White() { return RGBColor(255, 255, 255); }
};

// [BrightnessUtils e RGBColor rimangono identici a prima]
// ...

// --------------------------------------------------------------------
// --- CLASSI ATOMICHE: SIMPLE e DIMMABLE (non cambiano molto) ---
// --------------------------------------------------------------------

// Base per tutte le luci. Ora DimmableLight erediterà da questa.
class Light {
protected:
    const char* _name;
    bool _status = false;
public:
    Light(const char* name) : _name(name) {}
    virtual ~Light() {} // Buona pratica avere un distruttore virtuale
    const char* getName() const { return _name; }
    bool getStatus() const { return _status; }
    virtual void setStatus(bool status) = 0;
    void toggle() { setStatus(!_status); }
};


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
        if (status) {
            setBrightness(_last_user_percent);
        } else {
            setBrightness(0);
        }
    }
};

// Nuova classe: OUTSIDE LIGHT


// --------------------------------------------------------------------
// --- NUOVA RGBLight (USA LA COMPOSIZIONE) ---
// --------------------------------------------------------------------
class RGBLight {
private:
    const char* _name;
    // RGBLight ORA CONTIENE tre DimmableLight
    DimmableLight _channel_r;
    DimmableLight _channel_g;
    DimmableLight _channel_b;

public:
    // Il costruttore inizializza i suoi tre oggetti membri
    RGBLight(const char* name, uint8_t pin_r, uint8_t pin_g, uint8_t pin_b)
        : _name(name),
          _channel_r("Red", pin_r),
          _channel_g("Green", pin_g),
          _channel_b("Blue", pin_b)
    {
        // Il corpo del costruttore è vuoto, tutto avviene nella lista di inizializzazione
    }

    const char* getName() const { return _name; }

    // Controllo diretto per ogni canale (0-100%)
    void setRed(int percent) {
        _channel_r.setBrightness(percent);
    }
    void setGreen(int percent) {
        _channel_g.setBrightness(percent);
    }
    void setBlue(int percent) {
        _channel_b.setBrightness(percent);
    }
    
    // Metodo per impostare da una lista di colori hardcoded (RGBColor)
    void setColor(const RGBColor& color) {
        // Convertiamo i valori RAW (0-255) in percentuale utente (1-100)
        setRed(BrightnessUtils::toUser(color.r));
        setGreen(BrightnessUtils::toUser(color.g));
        setBlue(BrightnessUtils::toUser(color.b));
    }

    bool getStatus() const {
        // La luce è "ON" se almeno un canale è acceso
        return _channel_r.getStatus() || _channel_g.getStatus() || _channel_b.getStatus();
    }
    
    void setStatus(bool status) {
        if (status) {
            // Se accendo, cosa faccio? La cosa più sensata è accendere a bianco
            // o ripristinare l'ultimo stato. Per semplicità, accendiamo a bianco.
            setColor(RGBColor::White());
        } else {
            // Se spengo, spengo tutti i canali
            setColor(RGBColor::Off());
        }
    }
    
    void toggle() {
        setStatus(!getStatus());
    }
};

// AUTOMATIC LIGHT FOR OUTSIDE


class AutomaticLight {
public:
    // Modi di funzionamento
    enum LightMode { OFF, ON, AUTO_LIGHT, AUTO_LIGHT_MOVEMENT };

private:
    const char* _name;
    SimpleLight _light; // Contiene la luce fisica da controllare
    LightSensor& _lsensor; // Riferimento al sensore che usa per la logica AUTO
    MovementSensor& _msensor;
    
public:
    // Variabili pubbliche per un facile binding con LcdMenuLib
    LightMode currentMode = MODE_AUTO;
    int auto_threshold = 500; // Soglia LDR (0-1023) per l'accensione

public:
    AutomaticLight(const char* name, uint8_t pin, LightSensor& sensor)
        : _name(name), _light(name, pin), _sensor(sensor) {}

    const char* getName() const { return _name; }

    void setMode(LightMode mode) {
        currentMode = mode;
        // Quando cambiamo modo, applichiamo subito la logica
        update(); 
    }

    // Il cuore della logica. Da chiamare nel loop() principale.
    void update() {
        switch (currentMode) {
            case MODE_ON:
                _light.setStatus(true);
                break;
            case MODE_OFF:
                _light.setStatus(false);
                break;
            case MODE_AUTO:
                // Logica AUTO: se il livello di luce è sotto la soglia, accendi.
                if (_sensor.getLightLevel() < auto_threshold) {
                    _light.setStatus(true);
                } else {
                    _light.setStatus(false);
                }
                break;
        }
    }
};