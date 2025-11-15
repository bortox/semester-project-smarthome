#pragma once

#include <Arduino.h>
#include "lights.h" // Dipende dalle classi Light
#include "sensors.h" // Dipende dalle classi Sensor

// Costante per il debounce dei pulsanti, usata da più controller
constexpr unsigned long DEBOUNCE_DELAY = 50; // 50 millisecondi

/**
 * @class LightController
 * @brief Classe base astratta per qualsiasi oggetto che controlla una luce.
 *        Definisce un'interfaccia comune per essere gestita nel loop principale.
 */
class LightController {
public:
    virtual ~LightController() {}
    virtual void update() = 0; // Metodo che verrà chiamato ad ogni ciclo del loop
};

/**
 * @class ButtonToggleController
 * @brief Controlla una luce tramite un pulsante. Ogni click (al rilascio) esegue il toggle.
 */
class ButtonToggleController : public LightController {
private:
    Light* _light; // Puntatore alla luce da controllare
    uint8_t _pin;
    int _lastButtonState = HIGH; // Stato precedente (HIGH = non premuto con PULLUP)
    unsigned long _lastDebounceTime = 0;
    int _buttonState;

public:
    ButtonToggleController(Light* light, uint8_t buttonPin) 
        : _light(light), _pin(buttonPin) {
        pinMode(_pin, INPUT_PULLUP);
    }

    void update() override {
        int reading = digitalRead(_pin);

        // Se lo stato è cambiato, resetta il timer del debounce
        if (reading != _lastButtonState) {
            _lastDebounceTime = millis();
        }

        // Se è passato abbastanza tempo dal cambio di stato
        if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
            // Se lo stato è cambiato, agisci
            if (reading != _buttonState) {
                _buttonState = reading;
                // Agiamo solo quando il pulsante viene RILASCIATO (passa da LOW a HIGH)
                // per evitare azioni multiple se tenuto premuto.
                if (_buttonState == HIGH) {
                    _light->toggle();
                }
            }
        }
        _lastButtonState = reading;
    }
};

/**
 * @class PotentiometerDimmerController
 * @brief Controlla la luminosità di una luce dimmerabile tramite un potenziometro.
 *        Usa un template per essere compatibile con DimmableLight, RGBLight, etc.
 */
template<class DimmableType>
class PotentiometerDimmerController : public LightController {
private:
    DimmableType* _light; // La luce dimmerabile da controllare
    uint8_t _pin;
    int _lastMappedValue = -1; // -1 per forzare l'aggiornamento al primo ciclo
    const int THRESHOLD = 2; // Agisce solo se il valore cambia di almeno 2 (su 100) per evitare "tremolii"

public:
    PotentiometerDimmerController(DimmableType* light, uint8_t analogPin)
        : _light(light), _pin(analogPin) {}

    void update() override {
        int rawValue = analogRead(_pin);
        int mappedValue = map(rawValue, 0, 1023, 0, 100);

        if (abs(mappedValue - _lastMappedValue) >= THRESHOLD) {
            _light->setBrightness(mappedValue);
            _lastMappedValue = mappedValue;
        }
    }
};


/**
 * @class OutsideController
 * @brief Gestisce la logica di una luce esterna automatica basata su luce ambientale e movimento.
 *        Questa classe sostituisce la vecchia `AutomaticLight`.
 */
class OutsideController : public LightController {
public:
    enum Mode { MODE_OFF, MODE_ON, MODE_AUTO_LIGHT, MODE_AUTO_MOTION };

private:
    SimpleLight& _light; // La luce fisica da controllare
    LightSensor& _lightSensor;
    MovementSensor& _movementSensor;
    Mode _currentMode = MODE_AUTO_LIGHT;
    
    unsigned long _motion_trigger_time = 0;
    const unsigned long MOTION_LIGHT_DURATION = 30000; // 30 secondi

public:
    int light_threshold = 50; // Soglia in % per l'accensione (pubblica per essere modificata, es. da un menu)

    OutsideController(SimpleLight& light, LightSensor& ls, MovementSensor& ms)
        : _light(light), 
          _lightSensor(ls), 
          _movementSensor(ms)
    {}

    void setMode(Mode mode) {
        _currentMode = mode;
        update(); // Applica subito la logica del nuovo modo
    }

    Mode getMode() const { return _currentMode; }
    
    // Il cuore della logica, da chiamare nel loop() principale.
    void update() override {
        switch (_currentMode) {
            case MODE_ON:
                _light.setStatus(true);
                break;
            case MODE_OFF:
                _light.setStatus(false);
                break;
            case MODE_AUTO_LIGHT:
                _light.setStatus(_lightSensor.getValue() < light_threshold);
                break;
            case MODE_AUTO_MOTION:
                if (_lightSensor.getValue() < light_threshold) {
                    if (_movementSensor.getValue()) {
                        _light.setStatus(true);
                        _motion_trigger_time = millis(); // Resetta il timer
                    } else if (millis() - _motion_trigger_time > MOTION_LIGHT_DURATION) {
                        _light.setStatus(false); // Spegni dopo il timeout
                    }
                } else {
                    _light.setStatus(false); // Se c'è luce, la luce deve essere spenta
                }
                break;
        }
    }
};