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
  if (heap_end == 0) {
      heap_end = &__heap_start;
  }
  
  int static_data_size = (int)&__heap_start - RAMSTART;
  int heap_size = heap_end - &__heap_start;
  int stack_size = RAMEND - (int)&free_mem + 1;

  Serial.println(F(""));
  Serial.println(F("--- Memory Report ---"));
  Serial.print(F("Total RAM:        ")); Serial.print(RAMEND - RAMSTART + 1); Serial.println(F(" B"));
  Serial.println(F("---------------------"));
  Serial.print(F("Static Data:      ")); Serial.print(static_data_size); Serial.println(F(" B"));
  Serial.print(F("Heap Used:        ")); Serial.print(heap_size); Serial.println(F(" B"));
  Serial.print(F("Stack Used:       ")); Serial.print(stack_size); Serial.println(F(" B"));
  Serial.print(F("Free (Heap-Stack):  ")); Serial.print(free_mem); Serial.println(F(" B"));
  Serial.println(F("---------------------"));

  if (free_mem < 256) {
      Serial.println(F("!!! WARNING: Low free memory !!!"));
  }
}