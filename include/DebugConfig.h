#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

#include <Arduino.h>

// Abilita/disabilita debug globalmente
// Per produzione: cambiare a 0 o compilare con -DDEBUG_SERIAL=0
#ifndef DEBUG_SERIAL
  #define DEBUG_SERIAL 1
#endif

#if DEBUG_SERIAL
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINT_F(x) Serial.print(F(x))
  #define DEBUG_PRINTLN_F(x) Serial.println(F(x))
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT_F(x)
  #define DEBUG_PRINTLN_F(x)
#endif

#endif
