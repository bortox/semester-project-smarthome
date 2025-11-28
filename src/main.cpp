#include <Arduino.h>
extern "C" {
  #include "i2cmaster.h"
  #include "lcd.h"
}
#include "FlexibleMenu.h"
#include "MemoryMonitor.h"
#include "PhysicalInput.h"

void setup() {
    Serial.begin(9600);
    while (!Serial);
    
    i2c_init();
    LCD_init();
    LCD_backlight();
    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_str("Booting..."); // Stringa RAM temporanea, OK

    NavigationManager::instance().setLCD();

    // Crea dispositivi
    Serial.println(F("\n=== Creating Devices ==="));
    DeviceFactory::createSimpleLight("LED 1", 4);
    DeviceFactory::createDimmableLight("LED 2", 5);
    
    // Nuovi dispositivi
    DeviceFactory::createRGBLight("RGB Strip", 9, 10, 11);
    
    // Sensori per luce esterna
    static LightSensor photo("Photo", A6); // Static per evitare distruzione
    static MovementSensor pir("PIR", 8);
    DeviceFactory::createOutsideLight("Garden", 7, &photo, &pir);

    // FIX: Aggiungi sensore di temperatura
    DeviceFactory::createTemperatureSensor("Temp Sala");
    
    // FIX: Aggiungi sensore di luce fotoresistore
    DeviceFactory::createPhotoresistorSensor("Luce Stanza", A7);

    // Collega bottoni ai LED
    Serial.println(F("\n=== Setting up Buttons ==="));
    DeviceRegistry& registry = DeviceRegistry::instance();
    
    ButtonInput* btn1 = new ButtonInput(A0, 1, registry.getDevices()[0], ButtonMode::ACTIVE_LOW);
    ButtonInput* btn2 = new ButtonInput(A1, 2, registry.getDevices()[1], ButtonMode::ACTIVE_LOW);
    ButtonInput* btn3 = new ButtonInput(A2, 3, registry.getDevices()[2], ButtonMode::ACTIVE_LOW);
    ButtonInput* btn4 = new ButtonInput(A3, 4, registry.getDevices()[3], ButtonMode::ACTIVE_LOW);
    
    InputManager::instance().registerButton(btn1);
    InputManager::instance().registerButton(btn2);
    InputManager::instance().registerButton(btn3);
    InputManager::instance().registerButton(btn4);

    // Costruisci menu
    Serial.println(F("\n=== Building Menu ==="));
    MenuPage* mainMenu = MenuBuilder::buildMainMenu();
    NavigationManager::instance().initialize(mainMenu);

    // Report memoria
    Serial.println(F("\n=== Memory Report ==="));
    printMemoryReport();

    Serial.println(F("\n=== System Ready ==="));
    Serial.println(F("Commands: w=UP s=DOWN e=SELECT q=BACK"));
}

void loop() {
    // Aggiorna bottoni fisici
    InputManager::instance().updateAll();
    
    // FIX: Devices si aggiornano autonomamente
    static unsigned long lastDeviceUpdate = 0;
    if (millis() - lastDeviceUpdate > 100) {
        auto& registry = DeviceRegistry::instance();
        for (size_t i = 0; i < registry.getDevices().size(); i++) {
            registry.getDevices()[i]->update();
        }
        lastDeviceUpdate = millis();
    }

    // Gestione input seriale
    if (Serial.available()) {
        char input = Serial.read();
        while (Serial.available()) Serial.read();
        
        // Traduzione comandi
        if (input == 'w') input = 'U';
        else if (input == 's') input = 'D';
        else if (input == 'e') input = 'E';
        else if (input == 'q') input = 'B';
        
        // FIX: NavigationManager gestisce tutto e forza ridisegno
        NavigationManager::instance().handleInput(input);
    }

    // Ridisegna SOLO se necessario
    NavigationManager::instance().update();

    delay(50);
}