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
    
    // Inizializza I2C
    i2c_init();
    
    // Inizializza LCD
    LCD_init();
    LCD_backlight();
    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_str("Booting...");

    NavigationManager::instance().setLCD();

    // Crea dispositivi
    Serial.println(F("\n=== Creating Devices ==="));
    DeviceFactory::createSimpleLight("LED 1", 4);
    DeviceFactory::createDimmableLight("LED 2", 5);
    DeviceFactory::createDimmableLight("LED 3", 6);
    DeviceFactory::createSimpleLight("LED 4", 7);
    DeviceFactory::createTemperatureSensor("Temp");

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
    
    // Aggiorna sensori ogni 2 secondi
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate > 2000) {
        auto& registry = DeviceRegistry::instance();
        for (size_t i = 0; i < registry.getDevices().size(); i++) {
            registry.getDevices()[i]->update();
        }
        lastSensorUpdate = millis();
    }

    // Gestisci input seriale
    if (Serial.available()) {
        char input = Serial.read();
        NavigationManager& nav = NavigationManager::instance();
        MenuPage* current = nav.getCurrentPage();
        
        char menuCommand = input;
        if (input == 'w') menuCommand = 'U';
        else if (input == 's') menuCommand = 'D';
        else if (input == 'e') menuCommand = 'E';
        else if (input == 'q') menuCommand = 'B';
        
        if (menuCommand == 'B') {
            nav.navigateBack();
        } 
        else if (menuCommand == 'E') {
            size_t idx = current->getSelectedIndex();
            if (idx < current->getItemsCount()) {
                MenuItem* item = current->getItem(idx);
                if (item->getType() == MenuItemType::SUBMENU) {
                    SubMenuItem* sub = static_cast<SubMenuItem*>(item);
                    nav.navigateTo(sub->getTarget());
                } else {
                    item->handleInput(menuCommand);
                }
            }
        }
        else if (menuCommand == 'U' || menuCommand == 'D') {
            size_t idx = current->getSelectedIndex();
            if (idx < current->getItemsCount()) {
                MenuItem* item = current->getItem(idx);
                item->handleInput(menuCommand);
                
                if (!current->needsRedraw()) {
                    current->handleInput(menuCommand);
                }
            } else {
                current->handleInput(menuCommand);
            }
        }
        else {
            current->handleInput(menuCommand);
        }
        
        while (Serial.available()) Serial.read();
    }

    // Renderizza display
    NavigationManager::instance().update();

    delay(50);
}