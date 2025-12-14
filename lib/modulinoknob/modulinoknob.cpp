/**
 * ModulinoKnob.cpp
 * Implementazione logica driver
 */

#include "modulinoknob.h"

// Indirizzi possibili (shiftati a sinistra di 1 per i2cmaster)
// 0x74 << 1 = 0xE8
// 0x76 << 1 = 0xEC
#define ADDR_DEFAULT_1 0xE8
#define ADDR_DEFAULT_2 0xEC

ModulinoKnob::ModulinoKnob() {
    _i2c_addr = 0;
    _set_bug_detected = false;
    _encoder_value = 0;
    _last_encoder_value = 0;
    
    _btn_pressed = false;
    _btn_long_press_handled = false;
    _click_count = 0;
    _last_press_time = 0;
    _last_release_time = 0;
}

bool ModulinoKnob::begin() {
    // Inizializza la libreria i2cmaster
    i2c_init(); 

    // Tentativo di rilevamento su indirizzo 1
    if (i2c_start(ADDR_DEFAULT_1 | I2C_WRITE) == 0) {
        _i2c_addr = ADDR_DEFAULT_1;
        i2c_stop();
    } 
    // Tentativo di rilevamento su indirizzo 2
    else if (i2c_start(ADDR_DEFAULT_2 | I2C_WRITE) == 0) {
        _i2c_addr = ADDR_DEFAULT_2;
        i2c_stop();
    } else {
        return false; // Nessun modulo trovato
    }

    // Bug Detection Logic (dal codice Python originale)
    // Imposta a 100, se leggendo ottiene -100 o altro, attiva il flag bug
    get(); // Leggi e scarta per pulire buffer
    set(100);
    delay(10); // Piccolo delay per stabilità
    int16_t check = get();
    
    if (check != 100) {
        _set_bug_detected = true;
    }

    // Ripristina valore originale o azzera
    set(0);
    _encoder_value = 0;
    _last_encoder_value = 0;

    return true;
}

int16_t ModulinoKnob::get() {
    uint8_t buf[3];
    if (readData(buf, 3)) {
        // Converte in int16_t (Little Endian)
        int16_t val = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
        return val;
    }
    return _encoder_value; // Ritorna ultimo valore noto in caso di errore
}

void ModulinoKnob::set(int16_t value) {
    int16_t target = value;
    if (_set_bug_detected) {
        target = -value;
    }

    uint8_t buf[2];
    buf[0] = (uint8_t)(target & 0xFF);
    buf[1] = (uint8_t)((target >> 8) & 0xFF);
    
    writeData(buf, 2);
    _encoder_value = value;
    _last_encoder_value = value;
}

KnobEvent ModulinoKnob::update() {
    KnobEvent event = KnobEvent::NONE;
    uint8_t buf[3];
    unsigned long now = millis();

    // 1. Lettura Hardware via I2C
    if (!readData(buf, 3)) {
        return KnobEvent::NONE;
    }

    // Parsing dati
    int16_t raw_val = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    bool raw_btn = (buf[2] != 0);

    // Gestione bug firmware per lettura coerente
    _encoder_value = raw_val; // Assumiamo che la lettura sia corretta, il bug è solo sul set

    // 2. Rilevamento Rotazione (SCROLL)
    // Gestione overflow/underflow per rotazione continua
    int16_t diff = _encoder_value - _last_encoder_value;
    
    // Filtro per overflow (es. da 32767 a -32768)
    if (diff < -1000) diff = 1; // Ha fatto il giro in avanti
    else if (diff > 1000) diff = -1; // Ha fatto il giro indietro

    if (diff != 0) {
        if (diff > 0) event = KnobEvent::DOWN; // Orario -> Giù/Avanti
        else event = KnobEvent::UP;            // Antiorario -> Su/Indietro
        
        _last_encoder_value = _encoder_value;
        return event; // Priorità alla rotazione
    }

    // 3. Logica Pulsanti (Enter vs Back)
    
    // Fronte di salita (Press)
    if (raw_btn && !_btn_pressed) {
        _btn_pressed = true;
        _last_press_time = now;
        _btn_long_press_handled = false;
    }

    // Durante la pressione (Check Long Press)
    if (_btn_pressed && !_btn_long_press_handled) {
        if ((now - _last_press_time) > LONG_PRESS_MS) {
            event = KnobEvent::BACK; // Long Press -> BACK
            _btn_long_press_handled = true; // Evita di triggerare ancora
            _click_count = 0; // Resetta conteggi click
        }
    }

    // Fronte di discesa (Release)
    if (!raw_btn && _btn_pressed) {
        _btn_pressed = false;
        
        if (!_btn_long_press_handled) {
            // È stato un click breve
            _click_count++;
            _last_release_time = now;
        }
    }

    // Gestione Timing Click multipli
    if (_click_count > 0 && !_btn_pressed) {
        unsigned long wait_time = now - _last_release_time;

        if (_click_count == 1) {
            // Se è passato il tempo per il doppio click, è un SINGOLO CLICK
            if (wait_time > DOUBLE_CLICK_MS) {
                event = KnobEvent::ENTER;
                _click_count = 0;
            }
        }
        else if (_click_count >= 2) {
            // Doppio click rilevato immediatamente
            event = KnobEvent::BACK;
            _click_count = 0;
        }
    }

    return event;
}

// --- Private I2C Helpers using i2cmaster ---

bool ModulinoKnob::readData(uint8_t* buf, uint8_t count) {
    if (i2c_start(_i2c_addr | I2C_READ) != 0) {
        return false;
    }
    for (uint8_t i = 0; i < count; i++) {
        // Leggi ACK per tutti tranne l'ultimo byte, che vuole NAK
        if (i < count - 1) {
            buf[i] = i2c_readAck();
        } else {
            buf[i] = i2c_readNak();
        }
    }
    i2c_stop();
    return true;
}

bool ModulinoKnob::writeData(uint8_t* buf, uint8_t count) {
    if (i2c_start(_i2c_addr | I2C_WRITE) != 0) {
        return false;
    }
    for (uint8_t i = 0; i < count; i++) {
        i2c_write(buf[i]);
    }
    i2c_stop();
    return true;
}