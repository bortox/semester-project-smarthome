/**
 * @file MemoryMonitor.h
 * @brief Memory monitoring utilities for AVR microcontrollers
 * @author Andrea Bortolotti
 * @version 2.0
 * 
 * @details Provides functions to measure free RAM between heap and stack.
 * Essential for debugging memory issues on resource-constrained systems.
 * 
 * @note This file must remain header-only.
 * 
 * @ingroup Core
 */
#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <Arduino.h>

/**
 * @brief External reference to heap end marker
 * @details Provided by avr-libc, marks current end of heap
 */
extern int __heap_start;

/**
 * @brief External reference to break value
 * @details Provided by avr-libc, marks current heap allocation point
 */
extern int* __brkval;

/**
 * @brief Calculates free RAM between heap and stack
 * @return Free memory in bytes
 * 
 * @details Uses AVR memory layout to calculate space between
 * heap top (__brkval) and stack pointer (SP). Returns difference
 * between stack pointer and either __brkval or __heap_start.
 * 
 * @note Result may vary during execution as stack grows/shrinks
 */
inline int getFreeMemory() {
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

#endif