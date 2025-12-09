/**
 * @file DebugConfig.h
 * @brief Global debug configuration macros
 * @ingroup Core
 */
#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

#include <Arduino.h>

/**
 * @def DEBUG_SERIAL
 * @brief Enable/Disable Serial debug output
 * 
 * Set to 0 for production to save Flash/RAM.
 * Can be overridden via build flags.
 */
#ifndef DEBUG_SERIAL
  #define DEBUG_SERIAL 0
#endif

/**
 * @def DEBUG_I2C
 * @brief Enable/Disable I2C debugging via built-in LED
 * 
 * LED ON = I2C communication active
 * LED Stuck ON = I2C bus hang
 */
#ifndef DEBUG_I2C
  #define DEBUG_I2C 1
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
