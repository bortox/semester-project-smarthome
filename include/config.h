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
// Light pins (must be PWM pins for dimming functionality)
const uint8_t RGB_R_PIN  = 11;
const uint8_t RGB_G_PIN  = 10;
const uint8_t RGB_B_PIN  = 9;
const uint8_t L_ADJ = 3;
const uint8_t L_IN_ONOFF = 4;
const uint8_t L_OUT_ONOFF = 2;
// LCD Config
constexpr int LCD_COLS = 20;
constexpr int LCD_ROWS = 4;
constexpr int LCD_ADDRESS = 0x27;
// Sensor config
const uint8_t LDR_SENSOR_PIN  = A0; // Pin per il sensore di luminosità (fotoresistore)
const uint8_t PIR_SENSOR_PIN  = 13;  // Pin per il sensore di movimento PIR
// Button config
const uint8_t L_ADJ_BUTTON_PIN = 5;
const uint8_t L_IN_ONOFF_BUTTON_PIN = 6;
const uint8_t L_OUT_ONOFF_BUTTON_PIN = 7;
const uint8_t RGB_BUTTON_PIN = 8;

const uint8_t RGB_POTENTIOMETER = A1;
const uint8_t ADJ_POTENTIOMETER = A2;


// --- LIGHT OBJECTS ---
// Here we instantiate the objects representing the physical lights.
RGBLight      rgbLight("RGB Light", RGB_R_PIN, RGB_G_PIN, RGB_B_PIN);
DimmableLight dimmerLight("Spot Light", L_ADJ);
SimpleLight   outsideLight("External Light", L_OUT_ONOFF);
SimpleLight   insideLight("External Light", L_IN_ONOFF);

// --- SENSOR OBJECTS ---
LM75Sensor tempSensor("External Temp");
LightSensor    lightSensor("Luminosity", LDR_SENSOR_PIN);
MovementSensor pirSensor("Movement", PIR_SENSOR_PIN);

// --- CONTROLLERS FOR PHYSICAL BUTTONS ---
// These objects connect physical buttons to the lights.
ButtonToggleController rgbController(&rgbLight, RGB_BUTTON_PIN);
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
    MenuPage* insideLightPage = new MenuPage(insideLight.getName(), lightsPage);
    MenuPage* rgbColorPage = new MenuPage("Color Selection", rgbLightPage);

    lightsPage->addItem(new SubMenuItem(rgbLight.getName(), rgbLightPage));
    lightsPage->addItem(new SubMenuItem(dimmerLight.getName(), dimmerLightPage));
    lightsPage->addItem(new SubMenuItem(outsideLight.getName(), outsideLightPage));
    lightsPage->addItem(new SubMenuItem(insideLight.getName(), insideLightPage));

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

    insideLightPage->addItem(new LightToggleItem(&insideLight));


    // Content of the color page
    rgbColorPage->addItem(new ColorSelectItem("Warm White", &rgbLight, RGBColor::WarmWhite()));
    rgbColorPage->addItem(new ColorSelectItem("Ocean Blue", &rgbLight, RGBColor::OceanBlue()));

    // Content of the sensors page
    sensorsPage->addItem(new SensorDisplayItem<float, LM75Sensor>("Temp.", &tempSensor, "C"));

    return root;
}
