#pragma once // Prevent multiple inclusions
// Include all necessary libraries to define our home system
#include "MyMenu.h"
#include "lights.h"
#include "sensors.h"
#include "LightController.h"

// ===================================================================
// HARDWARE DEFINITION (PINS AND OBJECTS)
// This is the only place where you need to define the pins.
// ===================================================================
// Pins for physical buttons for direct control
const uint8_t BUTTON_RGB_PIN    = A0;
const uint8_t BUTTON_DIMMER_PIN = A1;
// Light pins (must be PWM pins for dimming functionality)
const uint8_t RGB_R_PIN  = 4;
const uint8_t RGB_G_PIN  = 5;
const uint8_t RGB_B_PIN  = 6;
const uint8_t DIMMER_PIN = 7;
constexpr int LCD_COLS = 20;
constexpr int LCD_ROWS = 4;
constexpr int LCD_ADDRESS = 0x27;
const uint8_t EXT_LIGHT_PIN   = 10;  // Pin per la luce esterna (non-PWM va bene)
const uint8_t LDR_SENSOR_PIN  = A5; // Pin per il sensore di luminosità (fotoresistore)
const uint8_t PIR_SENSOR_PIN  = 11;  // Pin per il sensore di movimento PIR


// --- LIGHT OBJECTS ---
// Here we instantiate the objects representing the physical lights.
RGBLight      rgbLight("LED Strip", RGB_R_PIN, RGB_G_PIN, RGB_B_PIN);
DimmableLight dimmerLight("Spot Light", DIMMER_PIN);
SimpleLight   outsideLight("External Light", EXT_LIGHT_PIN);

// --- SENSOR OBJECTS ---
LM75Sensor tempSensor("External Temp");
LightSensor    lightSensor("Luminosity", LDR_SENSOR_PIN);
MovementSensor pirSensor("Movement", PIR_SENSOR_PIN);

// --- CONTROLLERS FOR PHYSICAL BUTTONS ---
// These objects connect physical buttons to the lights.
ButtonToggleController rgbController(&rgbLight, BUTTON_RGB_PIN);
ButtonToggleController dimmerController(&dimmerLight, BUTTON_DIMMER_PIN);
OutsideController outsideController(outsideLight, lightSensor, pirSensor);

// ===================================================================
// MENU STRUCTURE CONSTRUCTION
// This function defines the entire user interface.
// ===================================================================
MenuItem* buildMenu() {
    // 1. Root of the menu
    MenuPage* root = new MenuPage("Main Menu");

    // 2. Main pages
    MenuPage* lightsPage = new MenuPage("Light Control", root);
    MenuPage* sensorsPage = new MenuPage("Sensor Status", root);

    root->addItem(new SubMenuItem("Lights", lightsPage));
    root->addItem(new SubMenuItem("Sensors", sensorsPage));

    // --- Specific pages for each light ---
    MenuPage* rgbLightPage = new MenuPage(rgbLight.getName(), lightsPage);
    MenuPage* dimmerLightPage = new MenuPage(dimmerLight.getName(), lightsPage);
    MenuPage* outsideLightPage = new MenuPage(outsideLight.getName(), lightsPage);
    MenuPage* rgbColorPage = new MenuPage("Color Selection", rgbLightPage);

    lightsPage->addItem(new SubMenuItem(rgbLight.getName(), rgbLightPage));
    lightsPage->addItem(new SubMenuItem(dimmerLight.getName(), dimmerLightPage));
    lightsPage->addItem(new SubMenuItem(outsideLight.getName(), outsideLightPage));

    // --- Content of light control pages ---

    // Controls for RGB light
    // NEW: Added LightToggleItem before brightness
    rgbLightPage->addItem(new LightToggleItem(&rgbLight));
    rgbLightPage->addItem(new ValueEditorItem<RGBLight>("Brightness", &rgbLight, rgbLightPage));
    rgbLightPage->addItem(new SubMenuItem("Color", rgbColorPage));

    // Controls for Dimmable light
    // NEW: Added LightToggleItem before brightness
    dimmerLightPage->addItem(new LightToggleItem(&dimmerLight));
    dimmerLightPage->addItem(new ValueEditorItem<DimmableLight>("Brightness", &dimmerLight, dimmerLightPage));

    // Controls for Outside Light
    outsideLightPage->addItem(new ModeSelectItem("Mode", &outsideController));
    // Per comodità, mostriamo anche lo stato attuale dei sensori in questa pagina
    outsideLightPage->addItem(new SensorDisplayItem<int, LightSensor>("Luminosity", &lightSensor, "%"));
    outsideLightPage->addItem(new SensorDisplayItem<bool, MovementSensor>("Movement", &pirSensor));


    // Content of the color page
    rgbColorPage->addItem(new ColorSelectItem("Warm White", &rgbLight, RGBColor::WarmWhite()));
    rgbColorPage->addItem(new ColorSelectItem("Ocean Blue", &rgbLight, RGBColor::OceanBlue()));

    // Content of the sensors page
    sensorsPage->addItem(new SensorDisplayItem<float, LM75Sensor>("Temp.", &tempSensor, "C"));

    return root;
}
