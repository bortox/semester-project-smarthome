#pragma once // Evita inclusioni multiple

// Includiamo tutte le librerie necessarie per definire la nostra casa
#include "MyMenu.h"
#include "lights.h"
#include "sensors.h"
#include "LightController.h"

// ===================================================================
// DEFINIZIONE HARDWARE (PIN E OGGETTI)
// Questo è l'unico posto dove dovrai definire i pin.
// ===================================================================

// Pin dei pulsanti fisici per il controllo diretto
const uint8_t BUTTON_RGB_PIN    = A0;
const uint8_t BUTTON_DIMMER_PIN = A1;

// Pin delle luci (devono essere pin PWM per le funzionalità di dimming)
const uint8_t RGB_R_PIN  = 4;
const uint8_t RGB_G_PIN  = 5;
const uint8_t RGB_B_PIN  = 6;
const uint8_t DIMMER_PIN = 7;

// --- OGGETTI LUCE ---
// Qui istanziamo gli oggetti che rappresentano le luci fisiche.
RGBLight      luceRGB("Striscia LED", RGB_R_PIN, RGB_G_PIN, RGB_B_PIN);
DimmableLight luceDimmer("Luce Spot", DIMMER_PIN);

// --- OGGETTI SENSORI ---
LM75Sensor sensoreTemp("Temp Esterna");

// --- CONTROLLER PER PULSANTI FISICI ---
// Questi oggetti collegano i pulsanti fisici alle luci.
ButtonToggleController controllerRGB(&luceRGB, BUTTON_RGB_PIN);
ButtonToggleController controllerDimmer(&luceDimmer, BUTTON_DIMMER_PIN);


// ===================================================================
// COSTRUZIONE DELLA STRUTTURA DEL MENU
// Questa funzione definisce l'intera interfaccia utente.
// ===================================================================
MenuItem* buildMenu() {
    // 1. Radice del menu
    MenuPage* root = new MenuPage("Menu Principale");

    // 2. Pagine principali
    MenuPage* lightsPage = new MenuPage("Controllo Luci", root);
    MenuPage* sensorsPage = new MenuPage("Stato Sensori", root);
    
    root->addItem(new SubMenuItem("Luci", lightsPage));
    root->addItem(new SubMenuItem("Sensori", sensorsPage));

    // --- Pagine specifiche per ogni luce ---
    MenuPage* pageLuceRGB = new MenuPage(luceRGB.getName(), lightsPage);
    MenuPage* pageLuceDimmer = new MenuPage(luceDimmer.getName(), lightsPage);
    MenuPage* pageColoreRGB = new MenuPage("Selez. Colore", pageLuceRGB);

    lightsPage->addItem(new SubMenuItem(luceRGB.getName(), pageLuceRGB));
    lightsPage->addItem(new SubMenuItem(luceDimmer.getName(), pageLuceDimmer));

    // --- Contenuto delle pagine di controllo luce ---
    
    // Controlli per la luce RGB
    // NUOVO: Aggiunto LightToggleItem prima della luminosità
    pageLuceRGB->addItem(new LightToggleItem(&luceRGB));
    pageLuceRGB->addItem(new ValueEditorItem<RGBLight>("Luminosita", &luceRGB, pageLuceRGB));
    pageLuceRGB->addItem(new SubMenuItem("Colore", pageColoreRGB));
    
    // Controlli per la luce Dimmerabile
    // NUOVO: Aggiunto LightToggleItem prima della luminosità
    pageLuceDimmer->addItem(new LightToggleItem(&luceDimmer));
    pageLuceDimmer->addItem(new ValueEditorItem<DimmableLight>("Luminosita", &luceDimmer, pageLuceDimmer));

    // Contenuto della pagina dei colori
    pageColoreRGB->addItem(new ColorSelectItem("Bianco Caldo", &luceRGB, RGBColor::WarmWhite()));
    pageColoreRGB->addItem(new ColorSelectItem("Oceano", &luceRGB, RGBColor::OceanBlue()));

    // Contenuto della pagina sensori
    sensorsPage->addItem(new SensorDisplayItem<float, LM75Sensor>("Temp.", &sensoreTemp, "C"));

    return root;
}