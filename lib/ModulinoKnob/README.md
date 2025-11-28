# ModulinoKnob Library

IMPORTANTE: QUESTA LIBRERIA VERRA' PORTATA IN C PURO SENZA USARE WIRE.H

Libreria semplificata per Arduino Modulino Knob (rotary encoder con pulsante).

## Features

- **Rotazione encoder**: Rileva direzione CW/CCW con debouncing
- **Click singolo**: Per conferma (ENTER)
- **Long press**: Pressione >800ms per tornare indietro (BACK)
- **Debouncing hardware**: Gestione automatica rimbalzi

## Utilizzo

```cpp
#include <ModulinoKnob.h>

ModulinoKnob knob(0x3A);  // Indirizzo I2C default

void setup() {
    Wire.begin();
    knob.begin();
}

void loop() {
    knob.update();  // Chiamare ogni loop!
    
    // Rotazione
    int8_t dir = knob.getDirection();
    if (dir > 0) {
        // Ruotato CW → Menu DOWN
    } else if (dir < 0) {
        // Ruotato CCW → Menu UP
    }
    
    // Click = ENTER
    if (knob.wasClicked()) {
        // Conferma selezione
    }
    
    // Long press = BACK
    if (knob.isLongPress()) {
        // Torna indietro nel menu
    }
}
```

## Indirizzi I2C

- **Default**: `0x3A` (0x74 >> 1)
- **Alternativo**: `0x3B` (0x76 >> 1)

## Timing

- **Debounce rotazione**: 30ms
- **Long press**: 800ms
- **Double click window**: 300ms (non usato per il menu)
