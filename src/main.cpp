/**

    @file main.cpp

    @brief Main application entry point

    @ingroup Core

    Orchestrates the initialization of devices, inputs, and the main event loop.

    Connects the physical layer (Buttons/Sensors) with the logic layer (Scenes)

    and the presentation layer (LCD Menu).
    */
    #include <Arduino.h>
    extern "C" {
    #include "i2cmaster.h"
    #include "lcd.h"
    }
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

    @brief System initialization

        Initializes I2C and LCD

        Creates devices via Factory

        Registers physical inputs (Buttons/Pots)

        Builds the dynamic menu structure
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
// Kitchen
DeviceFactory::createDimmableLight(F("Kitchen"), 3);
// Bedroom
DeviceFactory::createDimmableLight(F("Bedroom"), 5);
// Ambient Light
DeviceFactory::createRGBLight(F("Ambient Light"), 9, 10, 11);

// Outside / Garden
static LightSensor photo(F("Photo"), A6);
static MovementSensor pir(F("PIR"), 7);
DeviceFactory::createOutsideLight(F("Garden"), 6, &photo, &pir);

DeviceFactory::createTemperatureSensor(F("Outside Temp"));
DeviceFactory::createPhotoresistorSensor(F("Outside Light"), A6);
DeviceFactory::createPIRSensor(F("Motion PIR"), 7);

// ===== Create Virtual Sensors =====
DeviceFactory::createRamSensor(F("Free RAM"));
DeviceFactory::createVoltageSensor(F("VCC"));
DeviceFactory::createLoopTimeSensor(F("Loop Time"));

// ===== Setup Light Control Buttons =====
DeviceRegistry& registry = DeviceRegistry::instance();

// Living Room / Bedroom
ButtonInput* btnLiving = new ButtonInput(8, 1, registry.getDevices()[1], ButtonMode::ACTIVE_LOW);
InputManager::instance().registerButton(btnLiving);

// Kitchen
ButtonInput* btnKitchen = new ButtonInput(13, 2, registry.getDevices()[0], ButtonMode::ACTIVE_LOW);
InputManager::instance().registerButton(btnKitchen);

// Bedroom (Mapped to secondary Kitchen button pin from reference)
ButtonInput* btnBedroom = new ButtonInput(1, 3, registry.getDevices()[0], ButtonMode::ACTIVE_LOW);
InputManager::instance().registerButton(btnBedroom);

// Reserved / RGB
ButtonInput* btnReserved = new ButtonInput(16, 4, registry.getDevices()[2], ButtonMode::ACTIVE_LOW);
InputManager::instance().registerButton(btnReserved);

// Living Room Potentiometer
DimmableLight* outLight = static_cast<DimmableLight*>(registry.getDevices()[1]);
PotentiometerInput* pot2 = new PotentiometerInput(A0, outLight);
InputManager::instance().registerPotentiometer(pot2);

// RGB Potentiometer (Commented out in target, but reference uses A1)
DimmableLight* rgbLight = static_cast<DimmableLight*>(registry.getDevices()[2]);
PotentiometerInput* pot1 = new PotentiometerInput(A1, rgbLight);
InputManager::instance().registerPotentiometer(pot1);


// ===== Setup Menu Navigation Buttons =====
// UP 12 -> BACK 2
// DOWN 17 -> UP 12
// ENTER 4 -> ENTER
// BACK 2 -> DOWN 17
NavButtonInput* navUp = new NavButtonInput(17, InputEvent::UP, ButtonMode::ACTIVE_LOW);
NavButtonInput* navDown = new NavButtonInput(2, InputEvent::DOWN, ButtonMode::ACTIVE_LOW);
NavButtonInput* navSelect = new NavButtonInput(4, InputEvent::ENTER, ButtonMode::ACTIVE_LOW);
NavButtonInput* navBack = new NavButtonInput(12, InputEvent::BACK, ButtonMode::ACTIVE_LOW);

InputManager::instance().registerNavButton(navUp);
InputManager::instance().registerNavButton(navDown);
InputManager::instance().registerNavButton(navSelect);
InputManager::instance().registerNavButton(navBack);

// ===== Build Menu =====
MenuPage* mainMenu = MenuBuilder::buildMainMenu();

if (mainMenu) {
    NavigationManager::instance().initialize(mainMenu);

    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_str("System Ready!");
    delay(1000);
} else {
    // Critical Error Handler for memory exhaustion
    LCD_clear();
    LCD_set_cursor(0, 0);
    LCD_write_str("CRITICAL ERROR:");
    LCD_set_cursor(0, 1);
    LCD_write_str("Menu Alloc Failed");
    while(1); // Halt system
}

}

/**

    @brief Main execution loop

    Runs the system cycle:

        Poll Inputs

        Update Devices (Physics)

        Apply Scenes (Logic/Overrides)

        Render UI
        */
void loop() {

    unsigned long loopStart = micros();

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

    // 5. Register loop execution time for monitoring
    unsigned long loopEnd = micros();
    uint16_t loopDuration = (uint16_t)(loopEnd - loopStart);
    LoopTimeSensorDevice::registerLoopTime(loopDuration);
}