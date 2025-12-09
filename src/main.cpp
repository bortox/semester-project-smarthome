#include <Arduino.h>
extern "C" {
  #include "i2cmaster.h"
  #include "lcd.h"
}
#include "FlexibleMenu.h"
#include "MemoryMonitor.h"
#include "PhysicalInput.h"
#include "DebugConfig.h"

void setup() {
    // NOTE: Serial disabled - D0/D1 used for navigation buttons
    
#if DEBUG_I2C
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
#endif
    
    i2c_init();
    LCD_init();
    LCD_backlight();
    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_str("Booting...");

    NavigationManager::instance().setLCD();

    // ===== Create Devices =====
    DeviceFactory::createDimmableLight("Kitchen", 6);
    //DeviceFactory::createSimpleLight("Kitchen", 3);
    DeviceFactory::createDimmableLight("Bedroom", 5);
    DeviceFactory::createRGBLight("Ambient Light", 9, 10, 11);
    DeviceFactory::createTemperatureSensor("Outside Temp");

    
    // // ===== Setup Light Control Buttons =====
    DeviceRegistry& registry = DeviceRegistry::instance();
    
    // D12 -> Living Room (index 0)
    ButtonInput* btnLiving = new ButtonInput(13, 1, registry.getDevices()[0], ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnLiving);
    
    // D13 -> Kitchen (index 1)
    ButtonInput* btnKitchen = new ButtonInput(16, 2, registry.getDevices()[1], ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnKitchen);
    
    // D7 -> Bedroom (index 2)
    ButtonInput* btnBedroom = new ButtonInput(A6, 3, registry.getDevices()[2], ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnBedroom);
    
    // D8 -> Reserved (no action for now)
    ButtonInput* btnReserved = new ButtonInput(A7, 4, nullptr, ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnReserved);


    DimmableLight* rgbLight = static_cast<DimmableLight*>(registry.getDevices()[2]);
    PotentiometerInput* pot2 = new PotentiometerInput(A0, rgbLight);
    InputManager::instance().registerPotentiometer(pot2);

    static LightSensor photo("Photo", A3);
    static MovementSensor pir("PIR", 7);
    DeviceFactory::createOutsideLight("Garden", 3, &photo, &pir);

    // Aggiungi sensore di luce fotoresistore
    DeviceFactory::createPhotoresistorSensor("Outside Light", A3);
    
    // NUOVO: Aggiungi sensore di movimento PIR
    DeviceFactory::createPIRSensor("Motion PIR", 7);



    // ===== Setup Menu Navigation Buttons =====
    NavButtonInput* navUp = new NavButtonInput(12, 'U', ButtonMode::ACTIVE_LOW); // R
    NavButtonInput* navDown = new NavButtonInput(1, 'D', ButtonMode::ACTIVE_LOW);
    NavButtonInput* navSelect = new NavButtonInput(4, 'E', ButtonMode::ACTIVE_LOW);
    NavButtonInput* navBack = new NavButtonInput(2, 'B', ButtonMode::ACTIVE_LOW);
    
    InputManager::instance().registerNavButton(navUp);
    InputManager::instance().registerNavButton(navDown);
    InputManager::instance().registerNavButton(navSelect);
    InputManager::instance().registerNavButton(navBack);

    // ===== Build Menu =====
    MenuPage* mainMenu = MenuBuilder::buildMainMenu();
    NavigationManager::instance().initialize(mainMenu);

    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_str("System Ready!");
    delay(1000);
}

void loop() {
    // Update all inputs (device buttons + navigation buttons)
    InputManager::instance().updateAll();
    
    // Update all devices
    DeviceRegistry& registry = DeviceRegistry::instance();
    for (uint8_t i = 0; i < registry.getDevices().size(); i++) {
        registry.getDevices()[i]->update();
    }

    // Update menu display (only redraws if needed)
    NavigationManager::instance().update();
}