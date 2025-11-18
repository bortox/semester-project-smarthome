#pragma once
#include "MyMenu.h"
#include "lights.h"
#include "sensors.h"
#include "LightController.h"

// ===================================================================
// HARDWARE DEFINITION PER TEST BOARD
// ===================================================================
// LED dimmerabili sui pin 4,5,6,7
const uint8_t TEST_LED1 = 4;
const uint8_t TEST_LED2 = 5; 
const uint8_t TEST_LED3 = 6;
const uint8_t TEST_LED4 = 7;

// LCD Config
constexpr int LCD_COLS = 20;
constexpr int LCD_ROWS = 4;
constexpr int LCD_ADDRESS = 0x27;

// Bottoni di test su A0-A3
const uint8_t BUTTON1_PIN = A0;
const uint8_t BUTTON2_PIN = A1;
const uint8_t BUTTON3_PIN = A2;
const uint8_t BUTTON4_PIN = A3;

// ===================================================================
// OGGETTI PER TEST - OTTIMIZZATI
// ===================================================================

// --- LUCI DI TEST ---
DimmableLight testLed1("LED 1", TEST_LED1);
DimmableLight testLed2("LED 2", TEST_LED2);
DimmableLight testLed3("LED 3", TEST_LED3); 
DimmableLight testLed4("LED 4", TEST_LED4);

// --- SENSORI ---
LM75Sensor tempSensor("Temperature");

// Sensori fittizi per compatibilità
LightSensor    lightSensor("Luminosity", A0);
MovementSensor pirSensor("Movement", A1);

// --- CONTROLLERS ---
ButtonToggleController btn1(&testLed1, BUTTON1_PIN);
ButtonToggleController btn2(&testLed2, BUTTON2_PIN);
ButtonToggleController btn3(&testLed3, BUTTON3_PIN);
ButtonToggleController btn4(&testLed4, BUTTON4_PIN);

// ===================================================================
// MENU STATICO - PRE-ALLOCATO
// ===================================================================
class StaticMenuSystem {
private:
    // Tutte le pagine pre-allocate
    MenuPage root;
    MenuPage lightsPage;
    MenuPage sensorsPage;
    MenuPage led1Page, led2Page, led3Page, led4Page;
    
    // Tutti gli items pre-allocati  
    SubMenuItem subLights, subSensors;
    SubMenuItem subLed1, subLed2, subLed3, subLed4;
    LightToggleItem toggleLed1, toggleLed2, toggleLed3, toggleLed4;
    ValueEditorItem<DimmableLight> brightnessLed1, brightnessLed2, brightnessLed3, brightnessLed4;
    SensorDisplayItem<float, LM75Sensor> tempDisplay;

public:
    StaticMenuSystem() :
        // Inizializza pagine
        root("Main Menu"),
        lightsPage("Light Control", &root),
        sensorsPage("Sensor Status", &root),
        led1Page("LED 1", &lightsPage),
        led2Page("LED 2", &lightsPage), 
        led3Page("LED 3", &lightsPage),
        led4Page("LED 4", &lightsPage),
        
        // Inizializza submenu
        subLights("Lights", &lightsPage),
        subSensors("Sensors", &sensorsPage),
        subLed1("LED 1", &led1Page),
        subLed2("LED 2", &led2Page),
        subLed3("LED 3", &led3Page), 
        subLed4("LED 4", &led4Page),
        
        // Inizializza toggle items
        toggleLed1(&testLed1),
        toggleLed2(&testLed2),
        toggleLed3(&testLed3),
        toggleLed4(&testLed4),
        
        // Inizializza editor brightness
        brightnessLed1("Brightness", &testLed1, &led1Page),
        brightnessLed2("Brightness", &testLed2, &led2Page),
        brightnessLed3("Brightness", &testLed3, &led3Page),
        brightnessLed4("Brightness", &testLed4, &led4Page),
        
        // Inizializza display sensori
        tempDisplay("Temperature", &tempSensor, "C")
    {
        buildMenuStructure();
    }
    
private:
    void buildMenuStructure() {
        // Root menu
        root.addItem(&subLights);
        root.addItem(&subSensors);
        
        // Lights page
        lightsPage.addItem(&subLed1);
        lightsPage.addItem(&subLed2); 
        lightsPage.addItem(&subLed3);
        lightsPage.addItem(&subLed4);
        
        // LED pages
        led1Page.addItem(&toggleLed1);
        led1Page.addItem(&brightnessLed1);
        
        led2Page.addItem(&toggleLed2);
        led2Page.addItem(&brightnessLed2);
        
        led3Page.addItem(&toggleLed3);
        led3Page.addItem(&brightnessLed3);
        
        led4Page.addItem(&toggleLed4);
        led4Page.addItem(&brightnessLed4);
        
        // Sensors page
        sensorsPage.addItem(&tempDisplay);
    }
    
public:
    MenuItem* getRoot() { return &root; }
};

// Istanza globale del menu statico
StaticMenuSystem staticMenu;

// ===================================================================
// FUNZIONE BUILD MENU
// ===================================================================
MenuItem* buildMenu() {
    return staticMenu.getRoot();
}