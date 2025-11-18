#pragma once

#include <Arduino.h>

// Variabili esterne fornite dal linker AVR-GCC.
extern char __heap_start;
extern char *__brkval; // Puntatore alla fine dell'heap (top)

// Funzione che calcola la memoria libera tra la fine dell'heap e l'inizio dello stack.
// È la metrica più importante per la stabilità.
static int getFreeMemory() {
  char top; // Variabile locale, il suo indirizzo è sul top dello stack
  return &top - (__brkval == 0 ? &__heap_start : __brkval);
}

// Stampa un report completo dello stato della RAM sulla porta Seriale.
static void printMemoryReport() {
  int free_mem = getFreeMemory();
  char *heap_end = __brkval;
  if (heap_end == 0) { // Se l'heap non è mai stato usato, punta all'inizio
      heap_end = &__heap_start;
  }
  
  // Calcolo delle varie sezioni della RAM
  int static_data_size = (int)&__heap_start - RAMSTART;
  int heap_size = heap_end - &__heap_start;
  int stack_size = RAMEND - (int)&free_mem + 1;

  Serial.println(F(""));
  Serial.println(F("--- Memory Report ---"));
  Serial.print(F("Total RAM:        ")); Serial.print(RAMEND - RAMSTART + 1); Serial.println(F(" B"));
  Serial.println(F("---------------------"));
  Serial.print(F("Static Data:      ")); Serial.print(static_data_size); Serial.println(F(" B"));
  Serial.print(F("Heap Used:        ")); Serial.print(heap_size); Serial.println(F(" B (Menu Objects)"));
  Serial.print(F("Stack Used:       ")); Serial.print(stack_size); Serial.println(F(" B (Function Calls)"));
  Serial.print(F("Free (Heap-Stack):  ")); Serial.print(free_mem); Serial.println(F(" B  <-- CRITICAL"));
  Serial.println(F("---------------------"));

  if (free_mem < 256) { // Una soglia di sicurezza ragionevole per un ATmega328
      Serial.println(F("!!! WARNING: Low free memory !!!"));
  }
}