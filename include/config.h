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

// --- LIGHT OBJECTS ---
// Here we instantiate the objects representing the physical lights.
RGBLight      rgbLight("LED Strip", RGB_R_PIN, RGB_G_PIN, RGB_B_PIN);
DimmableLight dimmerLight("Spot Light", DIMMER_PIN);

// --- SENSOR OBJECTS ---
LM75Sensor tempSensor("External Temp");

// --- CONTROLLERS FOR PHYSICAL BUTTONS ---
// These objects connect physical buttons to the lights.
ButtonToggleController rgbController(&rgbLight, BUTTON_RGB_PIN);
ButtonToggleController dimmerController(&dimmerLight, BUTTON_DIMMER_PIN);

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
    MenuPage* rgbColorPage = new MenuPage("Color Selection", rgbLightPage);

    lightsPage->addItem(new SubMenuItem(rgbLight.getName(), rgbLightPage));
    lightsPage->addItem(new SubMenuItem(dimmerLight.getName(), dimmerLightPage));

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

    // Content of the color page
    rgbColorPage->addItem(new ColorSelectItem("Warm White", &rgbLight, RGBColor::WarmWhite()));
    rgbColorPage->addItem(new ColorSelectItem("Ocean Blue", &rgbLight, RGBColor::OceanBlue()));

    // Content of the sensors page
    sensorsPage->addItem(new SensorDisplayItem<float, LM75Sensor>("Temp.", &tempSensor, "C"));

    return root;
}
