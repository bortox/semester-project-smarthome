/**
 * @file main.cpp
 * @brief Main application entry point
 * @ingroup Core
 * 
 * Orchestrates the initialization of devices, inputs, and the main event loop.
 * Connects the physical layer (Buttons/Sensors) with the logic layer (Scenes)
 * and the presentation layer (LCD Menu).
 */
#include <Arduino.h>
#include "i2cmaster.h"
#include "lcd.h"
#include "FlexibleMenu.h"
#include "MemoryMonitor.h"
#include "PhysicalInput.h"
#include "DebugConfig.h"
#include "Scenes.h"

// Global scene instances (needed for menu access)
NightModeScene nightMode;
PartyScene partyMode;
AlarmScene alarmMode;

/**
 * @brief System initialization
 * 
 * 1. Initializes I2C and LCD
 * 2. Creates devices via Factory
 * 3. Registers physical inputs (Buttons/Pots)
 * 4. Builds the dynamic menu structure
 */
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
    DeviceFactory::createDimmableLight("Bedroom", 5);
    DeviceFactory::createRGBLight("Ambient Light", 9, 10, 11);
    DeviceFactory::createTemperatureSensor("Outside Temp");
    static LightSensor photo("Photo", A3);
    static MovementSensor pir("PIR", 7);
    DeviceFactory::createOutsideLight("Garden", 8, &photo, &pir);

    DeviceFactory::createPhotoresistorSensor("Outside Light", A3);
    DeviceFactory::createPIRSensor("Motion PIR", 7);
    // ===== Setup Light Control Buttons =====
    DeviceRegistry& registry = DeviceRegistry::instance();
    
    ButtonInput* btnLiving = new ButtonInput(3, 1, registry.getDevices()[0], ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnLiving);
    
    ButtonInput* btnKitchen = new ButtonInput(A2, 2, registry.getDevices()[1], ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnKitchen);
    
    ButtonInput* btnBedroom = new ButtonInput(13, 3, registry.getDevices()[1], ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnBedroom);
    
    ButtonInput* btnReserved = new ButtonInput(A7, 4, registry.getDevices()[2], ButtonMode::ACTIVE_LOW);
    InputManager::instance().registerButton(btnReserved);

    DimmableLight* rgbLight = static_cast<DimmableLight*>(registry.getDevices()[2]);
    PotentiometerInput* pot2 = new PotentiometerInput(A0, rgbLight);
    InputManager::instance().registerPotentiometer(pot2);

    PotentiometerInput* pot1 = new PotentiometerInput(A1, rgbLight);
    InputManager::instance().registerPotentiometer(pot1);

    

    // ===== Setup Menu Navigation Buttons =====
    NavButtonInput* navUp = new NavButtonInput(12, InputEvent::UP, ButtonMode::ACTIVE_LOW);
    NavButtonInput* navDown = new NavButtonInput(1, InputEvent::DOWN, ButtonMode::ACTIVE_LOW);
    NavButtonInput* navSelect = new NavButtonInput(4, InputEvent::ENTER, ButtonMode::ACTIVE_LOW);
    NavButtonInput* navBack = new NavButtonInput(2, InputEvent::BACK, ButtonMode::ACTIVE_LOW);
    
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

/**
 * @brief Main execution loop
 * 
 * Runs the system cycle:
 * 1. Poll Inputs
 * 2. Update Devices (Physics)
 * 3. Apply Scenes (Logic/Overrides)
 * 4. Render UI
 */
void loop() {
    // 1. Update all inputs (highest priority - user commands)
    InputManager::instance().updateAll();
    
    // 2. Update all devices (base physics layer)
    DeviceRegistry& registry = DeviceRegistry::instance();
    for (uint8_t i = 0; i < registry.getDevices().size(); i++) {
        registry.getDevices()[i]->update();
    }

    // 3. Apply active scenes (overwrites device states by priority)
    SceneManager::instance().update();

    // 4. Update menu display (only redraws if needed)
    NavigationManager::instance().update();
}