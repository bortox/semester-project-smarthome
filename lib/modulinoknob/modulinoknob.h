/**
 * modulinoknob.h
 * Driver ottimizzato per Modulino Knob usando i2cmaster (Peter Fleury)
 * Gestisce Encoder e Logica Pulsanti avanzata (Enter, Back, Scroll).
 */

#ifndef MODULINO_KNOB_H
#define MODULINO_KNOB_H

#include <Arduino.h>
#include <stdint.h>
#include "i2cmaster.h" // Assicurati che i2cmaster.h sia nel path

// Eventi restituiti dalla funzione update()
enum class KnobEvent : uint8_t {
    NONE = 0,
    UP,           // Rotazione oraria
    DOWN,         // Rotazione antioraria
    ENTER,        // Click Singolo
    BACK          // Doppio Click o Pressione Lunga
};

class ModulinoKnob {
private:
    uint8_t _i2c_addr;
    bool _set_bug_detected;
    
    // Encoder State
    int16_t _encoder_value;
    int16_t _last_encoder_value;

    // Button Logic State
    bool _btn_pressed;
    bool _btn_long_press_handled;
    uint8_t _click_count;
    unsigned long _last_press_time;
    unsigned long _last_release_time;

    // Constanti configurabili (salvate in Flash implicitamente dal compilatore)
    static const uint16_t LONG_PRESS_MS = 600;
    static const uint16_t DOUBLE_CLICK_MS = 250;

    // Helpers I2C
    bool readData(uint8_t* buf, uint8_t count);
    bool writeData(uint8_t* buf, uint8_t count);

public:
    ModulinoKnob();

    /**
     * Inizializza il modulo I2C e il sensore.
     * @return true se il modulo è stato trovato.
     */
    bool begin();

    /**
     * Da chiamare nel loop(). Legge l'encoder e gestisce lo stato dei pulsanti.
     * Restituisce un evento se è successo qualcosa.
     */
    KnobEvent update();

    /**
     * Imposta il valore corrente dell'encoder
     */
    void set(int16_t value);

    /**
     * Restituisce il valore grezzo corrente
     */
    int16_t get();
};

#endif