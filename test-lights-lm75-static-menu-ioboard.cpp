#include <Arduino.h>
#include <Wire.h>      // Includiamo Wire.h per la gestione I2C
#include "lights.h"
#include "sensors.h"


// MENU SETUP
#include <LcdMenu.h>
#include <MenuScreen.h>
#include <display/LiquidCrystal_I2CAdapter.h>
#include <renderer/CharacterDisplayRenderer.h>
#include <input/KeyboardAdapter.h>


MENU_SCREEN(mainScreen, mainItems,
    ITEM_BASIC("Item 1"),
    ITEM_BASIC("Item 2"),
    ITEM_BASIC("Item 3"),
    ITEM_BASIC("Item 4"));

LiquidCrystal_I2C lcd(0x27, 20, 4);
LiquidCrystal_I2CAdapter lcdAdapter(&lcd);
CharacterDisplayRenderer renderer(&lcdAdapter, 16, 2);
LcdMenu menu(renderer);
KeyboardAdapter keyboard(&menu, &Serial);


// --- DEFINIZIONE PIN ---
#define PIN_LUCE_SOGGIORNO 5
#define PIN_LUCE_CUCINA    6
#define PIN_LUCE_GIARDINO  7
#define PIN_QUARTA_LUCE    4 // Usiamo uno dei pin RGB per il test

// --- ISTANZE GLOBALI DEGLI OGGETTI ---
// I costruttori vengono chiamati prima del setup(), ma ora sono "sicuri"
// perché non interagiscono con l'hardware.

// Sensori
LM75Sensor temp_sensor("Temperatura Esterna");

// Luci
SimpleLight luce_soggiorno("Luce Soggiorno", PIN_LUCE_SOGGIORNO);
SimpleLight luce_cucina("Luce Cucina", PIN_LUCE_CUCINA);
SimpleLight luce_giardino("Luce Giardino", PIN_LUCE_GIARDINO);
SimpleLight luce_test("Luce Test", PIN_QUARTA_LUCE);

// Creiamo un array di puntatori a Light per gestire facilmente tutte le luci.
// Grazie al polimorfismo, possiamo metterci dentro oggetti di tipo SimpleLight, DimmableLight, etc.
Light* all_lights[] = {
  &luce_test,
  &luce_soggiorno,
  &luce_cucina,
  &luce_giardino
};
const int NUM_LIGHTS = sizeof(all_lights) / sizeof(all_lights[0]);


// --- SETUP ---
// Qui inizializziamo tutto nell'ordine corretto.
void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("\n--- Inizio Test OOP con Sensore I2C ---");

  // 1. Inizializza il bus I2C (fondamentale, va fatto prima di usare qualsiasi dispositivo I2C)
  Wire.begin();
  Serial.println("Bus I2C inizializzato.");

  // 2. Ora inizializza i dispositivi che usano il bus I2C
  temp_sensor.begin();
  Serial.println("Sensore di temperatura LM75 inizializzato.");

  // 3. (Opzionale) Spegniamo tutte le luci all'avvio per partire da uno stato pulito
  for (int i = 0; i < NUM_LIGHTS; i++) {
    all_lights[i]->setStatus(false);
  }
  Serial.println("Tutte le luci spente. Setup completato.");
  // MENU SETUP THINGS
  renderer.begin();
  menu.setScreen(mainScreen);  
}


// --- LOOP ---
// Qui testiamo le funzionalità degli oggetti.
void loop() {
  static unsigned long last_light_toggle = 0;
  static unsigned long last_temp_read = 0;
  static int current_light_index = 0;

  // Blocco per far lampeggiare le luci in sequenza, ogni 1.5 secondi
  if (millis() - last_light_toggle > 250) {
    last_light_toggle = millis();

    // Spegne la luce precedente
    int prev_index = (current_light_index == 0) ? NUM_LIGHTS - 1 : current_light_index - 1;
    all_lights[prev_index]->setStatus(false);

    // Accende la luce corrente
    all_lights[current_light_index]->setStatus(true);
    
    Serial.print("Accesa luce: ");
    Serial.println(all_lights[current_light_index]->getName());

    // Passa alla prossima luce per il ciclo successivo
    current_light_index++;
    if (current_light_index >= NUM_LIGHTS) {
      current_light_index = 0;
    }
  }

  // Blocco per leggere e stampare la temperatura, ogni 5 secondi
  if (millis() - last_temp_read > 5000) {
    last_temp_read = millis();
    
    float temperatura = temp_sensor.getValue();
    
    Serial.print("\n------------------------\n");
    Serial.print("Lettura Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" C");
    Serial.print("------------------------\n");
  }
  keyboard.observe();

}