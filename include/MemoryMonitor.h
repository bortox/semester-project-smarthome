#pragma once
#include <Arduino.h>

extern char __heap_start;
extern char *__brkval;

static int getFreeMemory() {
    char top;
    return &top - (__brkval == 0 ? &__heap_start : __brkval);
}

static void printMemoryReport() {
    int free_mem = getFreeMemory();
    char *heap_end = __brkval;
    if (heap_end == 0) heap_end = &__heap_start;
    
    int static_data = (int)&__heap_start - RAMSTART;
    int heap_used = heap_end - &__heap_start;
    int stack_used = RAMEND - (int)&free_mem + 1;

    Serial.println(F("\n--- Memory ---"));
    Serial.print(F("Total: ")); Serial.print(RAMEND - RAMSTART + 1); Serial.println(F(" B"));
    Serial.print(F("Static: ")); Serial.print(static_data); Serial.println(F(" B"));
    Serial.print(F("Heap: ")); Serial.print(heap_used); Serial.println(F(" B"));
    Serial.print(F("Stack: ")); Serial.print(stack_used); Serial.println(F(" B"));
    Serial.print(F("FREE: ")); Serial.print(free_mem); Serial.println(F(" B"));
    
    if (free_mem < 256) Serial.println(F("!!! WARNING LOW MEMORY !!!"));
}